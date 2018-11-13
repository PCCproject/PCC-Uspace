from open_ai.common import explained_variance, zipsame, dataset
from open_ai import logger
import open_ai.common.tf_util as U
import tensorflow as tf, numpy as np
import time
from collections import deque
from open_ai.common.mpi_adam import MpiAdam
from open_ai.common.cg import cg
from contextlib import contextmanager
import sys
import os
import glob

def create_checkpoint(model_name, checkpoint_dir, checkpoint_num):
    if checkpoint_num == 0:
        os.system("mkdir -p " + checkpoint_dir)
    filenames = []
    filenames = glob.glob(model_name + "*")
    for filename in filenames:
        dot_pos = filename.rfind(".")
        slash_pos = filename.rfind("/")
        prefix = filename[slash_pos + 1:dot_pos]
        suffix = filename[dot_pos:]
        new_name = checkpoint_dir + prefix + "_" + str(checkpoint_num) + suffix
        os.system("cp %s %s" % (filename, new_name))

def traj_segment_generator(agg):
    while True:
        data = agg.poll_for_data().__next__()
        yield data

def add_vtarg_and_adv(seg, gamma, lam):
    new = np.append(seg["new"], 0)  # last element is only used for last vtarg, but we already zeroed it if last new = 1
    vpred = np.append(seg["vpred"], seg["nextvpred"])
    T = len(seg["rew"])
    seg["adv"] = gaelam = np.empty(T, 'float32')
    rew = seg["rew"]
    lastgaelam = 0
    for t in reversed(range(T)):
        nonterminal = 1 - new[t + 1]
        delta = rew[t] + gamma * vpred[t + 1] * nonterminal - vpred[t]
        gaelam[t] = lastgaelam = delta + gamma * lam * nonterminal * lastgaelam
    
    seg["tdlamret"] = seg["adv"] + seg["vpred"]

class TrpoTrainer():
    def __init__(self, data_agg, env, policy_func, *,
            max_kl, cg_iters,
            gamma, lam,
            entcoeff=0.0,
            cg_damping=1e-2,
            vf_stepsize=3e-4,
            vf_iters=3,
            max_timesteps=0, max_episodes=0, max_iters=0,
            callback=None,
            checkpoint_dir=None,
            checkpoint_freq=0
            ):

        self.use_lstm = False

        self.checkpoint_dir = checkpoint_dir
        self.checkpoint_freq = checkpoint_freq
        self.checkpoint_num = 0
        self.agg = data_agg

        self.max_kl = max_kl
        self.cg_iters = cg_iters
        self.max_iters = max_iters
        self.vf_stepsize = vf_stepsize
        self.vf_iters = vf_iters
        self.max_timesteps = max_timesteps
        self.max_episodes = max_episodes
        self.max_iters = max_iters

        self.gamma = gamma
        self.lam = lam

        self.ob_space = env.observation_space
        self.ac_space = env.action_space

        self.pi = policy_func("pi", self.ob_space, self.ac_space)
        self.oldpi = policy_func("oldpi", self.ob_space, self.ac_space)
        self.atarg = tf.placeholder(dtype=tf.float32, shape=[None])  # Target advantage function (if applicable)
        self.ret = tf.placeholder(dtype=tf.float32, shape=[None])  # Empirical return

        self.ob = U.get_placeholder_cached(name="ob")
        
        if self.use_lstm:
            self.h_state = U.get_placeholder_cached(name="h_state")
            self.c_state = U.get_placeholder_cached(name="c_state")
        
        self.ac = self.pi.pdtype.sample_placeholder([None])
        ac_summary = tf.summary.histogram("ac summary", self.ac)

        self.kloldnew = self.oldpi.pd.kl(self.pi.pd)
        self.ent = self.pi.pd.entropy()
        self.meankl = U.mean(self.kloldnew)
        self.meanent = U.mean(self.ent)
        self.entbonus = entcoeff * self.meanent

        self.vferr = U.mean(tf.square(self.pi.vpred - self.ret))

        self.ratio = tf.exp(self.pi.pd.logp(self.ac) - self.oldpi.pd.logp(self.ac))  # advantage * pnew / pold
        self.surrgain = U.mean(self.ratio * self.atarg)

        self.optimgain = self.surrgain + self.entbonus
        self.losses = [self.optimgain, self.meankl, self.entbonus, self.surrgain, self.meanent]
        self.loss_names = ["optimgain", "meankl", "entloss", "surrgain", "entropy"]

        self.dist = self.meankl

        self.all_var_list = self.pi.get_trainable_variables()
        self.var_list = [v for v in self.all_var_list if v.name.split("/")[1].startswith("pol")]
        self.var_summaries = []
        for v in self.var_list:
            self.var_summaries.append(tf.summary.tensor_summary(v.name, v))
        self.vf_var_list = [v for v in self.all_var_list if v.name.split("/")[1].startswith("vf")]
        self.vfadam = MpiAdam(self.vf_var_list)

        self.get_flat = U.GetFlat(self.var_list)
        self.set_from_flat = U.SetFromFlat(self.var_list)
        self.klgrads = tf.gradients(self.dist, self.var_list)
        self.flat_tangent = tf.placeholder(dtype=tf.float32, shape=[None], name="flat_tan")
        self.shapes = [var.get_shape().as_list() for var in self.var_list]
    
        start = 0
        self.tangents = []
        for shape in self.shapes:
            sz = U.intprod(shape)
            self.tangents.append(tf.reshape(self.flat_tangent[start:start + sz], shape))
            start += sz
        self.gvp = tf.add_n([U.sum(g * tangent) for (g, tangent) in zipsame(self.klgrads, self.tangents)])  # pylint: disable=E1111
        self.fvp = U.flatgrad(self.gvp, self.var_list)

        self.assign_old_eq_new = U.function([], [], updates=[tf.assign(oldv, newv)
                                                    for (oldv, newv) in
                                                    zipsame(self.oldpi.get_variables(), self.pi.get_variables())])
        # Edit for LSTM
        if self.use_lstm:
            self.compute_losses = U.function([self.ob, self.h_state, self.c_state, self.ac, self.atarg], self.losses)
            self.compute_lossandgrad = U.function([self.ob, self.h_state, self.c_state, self.ac, self.atarg], self.losses + [U.flatgrad(self.optimgain, self.var_list)])
            self.compute_fvp = U.function([self.flat_tangent, self.ob, self.h_state, self.c_state, self.ac, self.atarg], self.fvp)
            self.compute_vflossandgrad = U.function([self.ob, self.h_state, self.c_state, self.ret], U.flatgrad(self.vferr, self.vf_var_list))
        # Original
        else:
            self.compute_losses = U.function([self.ob, self.ac, self.atarg], self.losses)
            self.compute_lossandgrad = U.function([self.ob, self.ac, self.atarg], self.losses + [U.flatgrad(self.optimgain, self.var_list)])
            self.compute_fvp = U.function([self.flat_tangent, self.ob, self.ac, self.atarg], self.fvp)
            self.compute_vflossandgrad = U.function([self.ob, self.ret], U.flatgrad(self.vferr, self.vf_var_list))

        U.initialize()
        th_init = self.get_flat()
        self.set_from_flat(th_init)
        self.vfadam.sync()
        self.cg_damping = cg_damping


    def get_model(self):
        return self.pi

    def train(self, model_name):
        seg_gen = traj_segment_generator(self.agg)
        #summ_writer = tf.summary.FileWriter("/home/njay2/PCC/deep-learning/experiments/tf_summary/",
        #    tf.get_default_session().graph)
        #summ_writer.flush()
        
        @contextmanager
        def timed(msg):
            print(msg)
            tstart = time.time()
            yield
            print("done in %.3f seconds" % (time.time() - tstart))

        def allmean(x):
            assert isinstance(x, np.ndarray)
            return x

        episodes_so_far = 0
        timesteps_so_far = 0
        iters_so_far = 0
        tstart = time.time()
        lenbuffer = deque(maxlen=200)  # rolling buffer for episode lengths
        rewbuffer = deque(maxlen=200)  # rolling buffer for episode rewards

        assert sum([self.max_iters > 0, self.max_timesteps > 0, self.max_episodes > 0]) == 1

        saver = tf.train.Saver()

        while True:
            if self.max_timesteps and timesteps_so_far >= self.max_timesteps:
                break
            elif self.max_episodes and episodes_so_far >= self.max_episodes:
                break
            elif self.max_iters and iters_so_far >= self.max_iters:
                break
            logger.log("********** Iteration %i ************" % iters_so_far)

            if ("--no-training" not in sys.argv):
                saver.save(tf.get_default_session(), model_name)

            if self.checkpoint_freq > 0 and self.checkpoint_dir is not None:
                if iters_so_far % self.checkpoint_freq == 0:
                    create_checkpoint(model_name, self.checkpoint_dir, self.checkpoint_num)
                    self.checkpoint_num += 1
            
            with timed("sampling"):
                seg = seg_gen.__next__()

            for var in self.var_list:
                #print("%s: %s" % (var.name, tf.get_default_session().run(var)))
                pass

            for summ in self.var_summaries:
                #summ_writer.add_summary(summ.eval())
                #summ_writer.flush()
                pass

            if seg["stop"]:
                break

            add_vtarg_and_adv(seg, self.gamma, self.lam)

            ob, ac, atarg, tdlamret = seg["ob"], seg["ac"], seg["adv"], seg["tdlamret"]
     
            """
            if iters_so_far == 100:
                print('{"Experiment Parameters":{},"Log Version":"njay-1","Events":[')
                for i in range(0, len(ob)):
                    print('{"Name":"Decision","Number":%d,\n' % i)
                    print('"ob":"%s",\n' % str(ob[i]))
                    print('"ac":%f,\n' % ac[i])
                    print('"rew":%f,\n' % seg["rew"][i])
                    print('"adv":%f,\n' % atarg[i])
                    print('"vpred":%f,\n' % seg["vpred"][i])
                    print('"tdlamret":%f\n},' % tdlamret[i])
                print(']')
                exit(0)
            """

            vpredbefore = seg["vpred"]  # predicted value function before udpate
            atarg = (atarg - atarg.mean()) / atarg.std()  # standardized advantage function estimate

            if hasattr(self.pi, "ret_rms"): self.pi.ret_rms.update(tdlamret)
            if hasattr(self.pi, "ob_rms"): self.pi.ob_rms.update(ob)  # update running mean/std for policy

            args = None
            if self.use_lstm:
                args = seg["ob"], seg["h_state"], seg["c_state"], seg["ac"], atarg
            else:
                args = seg["ob"], seg["ac"], atarg
            fvpargs = [arr[::5] for arr in args]

            def fisher_vector_product(p):
                return allmean(self.compute_fvp(p, *fvpargs)) + self.cg_damping * p

            self.assign_old_eq_new()  # set old parameter values to new parameter values
            
            *lossbefore, g = self.compute_lossandgrad(*args)
            lossbefore = allmean(np.array(lossbefore))
            print("early g: " + str(g))
            
            g = allmean(g)

            if np.allclose(g, 0):
                logger.log("Got zero gradient. not updating")
            else:
                stepdir = cg(fisher_vector_product, g, cg_iters=self.cg_iters, verbose=True)

                if (not np.isfinite(stepdir).all()):
                    for k in range(0, 16):
                        print("seg[ob]: " + str(seg["ob"][k]))
                        print("seg[ac]: " + str(seg["ac"][k]))
                    print("g: " + str(g))
                    print("stepdir: " + str(stepdir))

                    print("optimgain" + str(self.optimgain))
                    print("var_list: " + str(self.var_list))
                assert np.isfinite(stepdir).all()
                shs = .5 * stepdir.dot(fisher_vector_product(stepdir))
                lm = np.sqrt(shs / self.max_kl)
                
                fullstep = stepdir / lm
                expectedimprove = g.dot(fullstep)
                surrbefore = lossbefore[0]
                stepsize = 1.0
                thbefore = self.get_flat()
                
                for _ in range(10):
                    thnew = thbefore + fullstep * stepsize
                    self.set_from_flat(thnew)
                    meanlosses = surr, kl, *_ = allmean(np.array(self.compute_losses(*args)))
                    improve = surr - surrbefore
                    logger.log("Expected: %.3f Actual: %.3f" % (expectedimprove, improve))
                    if not np.isfinite(meanlosses).all():
                        logger.log("Got non-finite value of losses -- bad!")
                    elif kl > self.max_kl * 1.5:
                        logger.log("violated KL constraint. shrinking step.")
                    elif improve < 0:
                        logger.log("surrogate didn't improve. shrinking step.")
                    else:
                        logger.log("Stepsize OK!")
                        break
                    stepsize *= .5
                else:
                    logger.log("couldn't compute a good step")
                    self.set_from_flat(thbefore)

            for _ in range(self.vf_iters):
                if self.use_lstm:
                    for (mbob, mbh, mbc, mbret) in dataset.iterbatches((seg["ob"],
                                                                        seg["h_state"],
                                                                        seg["c_state"],
                                                                        seg["tdlamret"]),
                                                             include_final_partial_batch=False, batch_size=64):
                        g = allmean(self.compute_vflossandgrad(mbob, mbh, mbc, mbret))
                        self.vfadam.update(g, self.vf_stepsize)
                else:
                    for (mbob, mbret, mbscale) in dataset.iterbatches((seg["ob"], seg["tdlamret"], seg["vpred_scale"]),
                                                             include_final_partial_batch=False, batch_size=64):
                        scaled_mbret = mbret / mbscale
                        g = allmean(self.compute_vflossandgrad(mbob, scaled_mbret))
                        self.vfadam.update(g, self.vf_stepsize)
            #logger.record_tabular("ev_tdlam_before", explained_variance(vpredbefore, tdlamret))

            lens = seg["ep_lens"]
            lenbuffer.extend(lens)
            rewbuffer.extend(seg["ep_rets"])

            logger.record_tabular("DensityRew", np.mean(rewbuffer) / np.mean(lenbuffer))
            logger.record_tabular("EpLenMean", np.mean(lenbuffer))
            logger.record_tabular("VpredMean", np.mean(seg["vpred"]))
            logger.record_tabular("tdlamret", np.mean(seg["tdlamret"]))
            episodes_so_far += len(lens)
            timesteps_so_far += sum(lens)
            iters_so_far += 1

            logger.record_tabular("TimeElapsed", time.time() - tstart)

            logger.dump_tabular()

def flatten_lists(listoflists):
    return [el for list_ in listoflists for el in list_]
