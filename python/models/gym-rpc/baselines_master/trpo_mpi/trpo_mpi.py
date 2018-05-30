from baselines_master.common import explained_variance, zipsame, dataset
from baselines import logger
import baselines.common.tf_util as U
import tensorflow as tf, numpy as np
import time
from baselines_master.common import colorize
from mpi4py import MPI
from collections import deque
from baselines_master.common.mpi_adam import MpiAdam
from baselines_master.common.cg import cg
from contextlib import contextmanager
import sys

_acs = []

def traj_segment_generator(agg):
    while True:
        #print("Getting dataset")
        data = agg.get_dataset()
        #print("Got dataset")
        #for i in range(0, len(data["ob"])):
        #    print(data["ob"][i])
        #    print(data["ac"][i])
        #    print(data["rew"][i])
        #exit(-1)
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
    def __init__(self, data_agg, agent, env, policy_func, *,
            timesteps_per_batch,
            max_kl, cg_iters,
            gamma, lam,
            entcoeff=0.0,
            cg_damping=1e-2,
            vf_stepsize=3e-4,
            vf_iters=3,
            max_timesteps=0, max_episodes=0, max_iters=0,
            callback=None
            ):

        self.agg = data_agg

        self.timesteps_per_batch = timesteps_per_batch
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
        if (not agent is None):
            agent.init(self.pi, timesteps_per_batch, self.ob_space.sample(), self.ac_space.sample())
        self.oldpi = policy_func("oldpi", self.ob_space, self.ac_space)
        self.atarg = tf.placeholder(dtype=tf.float32, shape=[None])  # Target advantage function (if applicable)
        self.ret = tf.placeholder(dtype=tf.float32, shape=[None])  # Empirical return

        self.ob = U.get_placeholder_cached(name="ob")
        self.ac = self.pi.pdtype.sample_placeholder([None])

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
        self.compute_losses = U.function([self.ob, self.ac, self.atarg], self.losses)
        self.compute_lossandgrad = U.function([self.ob, self.ac, self.atarg], self.losses + [U.flatgrad(self.optimgain, self.var_list)])
        self.compute_fvp = U.function([self.flat_tangent, self.ob, self.ac, self.atarg], self.fvp)
        self.compute_vflossandgrad = U.function([self.ob, self.ret], U.flatgrad(self.vferr, self.vf_var_list))

        U.initialize()
        th_init = self.get_flat()
        MPI.COMM_WORLD.Bcast(th_init, root=0)
        self.set_from_flat(th_init)
        self.vfadam.sync()
        #print("Init param sum", th_init.sum(), flush=True)
        self.cg_damping = cg_damping
    
    def train(self, model_name):
        nworkers = MPI.COMM_WORLD.Get_size()
        rank = MPI.COMM_WORLD.Get_rank()
        seg_gen = traj_segment_generator(self.agg)
        
        @contextmanager
        def timed(msg):
            if rank == 0:
                print(colorize(msg, color='magenta'))
                tstart = time.time()
                yield
                print(colorize("done in %.3f seconds" % (time.time() - tstart), color='magenta'))
            else:
                yield

        def allmean(x):
            assert isinstance(x, np.ndarray)
            out = np.empty_like(x)
            MPI.COMM_WORLD.Allreduce(x, out, op=MPI.SUM)
            out /= nworkers
            return out


        episodes_so_far = 0
        timesteps_so_far = 0
        iters_so_far = 0
        tstart = time.time()
        lenbuffer = deque(maxlen=40)  # rolling buffer for episode lengths
        rewbuffer = deque(maxlen=40)  # rolling buffer for episode rewards

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
                print("model saved to " + model_name)
                saver.save(tf.get_default_session(), model_name)
            #print("Getting dataset")
            with timed("sampling"):
                seg = seg_gen.__next__()
            #print("Got dataset")
            add_vtarg_and_adv(seg, self.gamma, self.lam)

            # ob, ac, atarg, ret, td1ret = map(np.concatenate, (obs, acs, atargs, rets, td1rets))
            #print("Extracting vars from dataset")
            ob, ac, atarg, tdlamret = seg["ob"], seg["ac"], seg["adv"], seg["tdlamret"]
      
            """
            print(len(ac))
            print(len(seg["rew"]))
            for i in range(0, len(ac)):
                print("ob = " + str(ob[i]))
                print("ac = " + str(ac[i]))
                print("rw = " + str(seg["rew"][i]))
            """
            #print("prediction and atarg")
            vpredbefore = seg["vpred"]  # predicted value function before udpate
            atarg = (atarg - atarg.mean()) / atarg.std()  # standardized advantage function estimate

            #print("Updating policy running values")
            if hasattr(self.pi, "ret_rms"): self.pi.ret_rms.update(tdlamret)
            if hasattr(self.pi, "ob_rms"): self.pi.ob_rms.update(ob)  # update running mean/std for policy

            #atarg *= 0.0
            #print("Constructing args")
            args = seg["ob"], seg["ac"], atarg
            #print("atarg = " + str(atarg))
            fvpargs = [arr[::5] for arr in args]

            def fisher_vector_product(p):
                return allmean(self.compute_fvp(p, *fvpargs)) + self.cg_damping * p

            #print("Assigning new to old")
            self.assign_old_eq_new()  # set old parameter values to new parameter values
            #print("args = " + str(len(args)))
            #print("Computing gradient")
            with timed("computegrad"):
                *lossbefore, g = self.compute_lossandgrad(*args)
            lossbefore = allmean(np.array(lossbefore))
            #print("Raw gradient = " + str(g))
            g = allmean(g)

            #print("Gradient = " + str(g))
            #print(var_list)
            #print(len(var_list))
            #print(len(g))
            s = tf.get_default_session()

            #my_vars = tf.global_variables()
            #print(my_vars)
            #exit(1)
            #for v in var_list:
            #    print(v.name + " = " + str(s.run(v)))
            
            if np.allclose(g, 0):
                logger.log("Got zero gradient. not updating")
            else:
                with timed("cg"):
                    stepdir = cg(fisher_vector_product, g, cg_iters=self.cg_iters, verbose=rank == 0)
                #print("Stepdir = " + str(stepdir))
                if (not np.isfinite(stepdir).all()):
                    print("seg[ob]: " + str(seg["ob"]))
                    print("seg[ac]: " + str(seg["ac"]))
                assert np.isfinite(stepdir).all()
                shs = .5 * stepdir.dot(fisher_vector_product(stepdir))
                lm = np.sqrt(shs / self.max_kl)
                # logger.log("lagrange multiplier:", lm, "gnorm:", np.linalg.norm(g))
                fullstep = stepdir / lm
                #print("Full step = " + str(fullstep))
                expectedimprove = g.dot(fullstep)
                surrbefore = lossbefore[0]
                stepsize = 1.0
                thbefore = self.get_flat()
                #print("thbefore = " + str(thbefore))
                for _ in range(10):
                    #print("stepsize = " + str(stepsize))
                    #print("this step = " + str(fullstep * stepsize))
                    thnew = thbefore + fullstep * stepsize
                    #print("thnew = " + str(thnew))
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
                if nworkers > 1 and iters_so_far % 20 == 0:
                    paramsums = MPI.COMM_WORLD.allgather((thnew.sum(), self.vfadam.getflat().sum()))  # list of tuples
                    assert all(np.allclose(ps, paramsums[0]) for ps in paramsums[1:])

            #for v in var_list:
            #    print(v.name + " = " + str(s.run(v)))
            for (lossname, lossval) in zip(self.loss_names, meanlosses):
                logger.record_tabular(lossname, lossval)

            with timed("vf"):

                for _ in range(self.vf_iters):
                    for (mbob, mbret) in dataset.iterbatches((seg["ob"], seg["tdlamret"]),
                                                             include_final_partial_batch=False, batch_size=64):
                        g = allmean(self.compute_vflossandgrad(mbob, mbret))
                        self.vfadam.update(g, self.vf_stepsize)

            logger.record_tabular("ev_tdlam_before", explained_variance(vpredbefore, tdlamret))

            lrlocal = (seg["ep_lens"], seg["ep_rets"])  # local values
            listoflrpairs = MPI.COMM_WORLD.allgather(lrlocal)  # list of tuples
            lens, rews = lrlocal #map(flatten_lists, zip(*listoflrpairs))
            lenbuffer.extend(lens)
            rewbuffer.extend(rews)

            logger.record_tabular("DensityRew", np.mean(rewbuffer) / np.mean(lenbuffer))
            logger.record_tabular("EpLenMean", np.mean(lenbuffer))
            logger.record_tabular("EpRewMean", np.mean(rewbuffer))
            logger.record_tabular("EpThisIter", len(lens))
            episodes_so_far += len(lens)
            timesteps_so_far += sum(lens)
            iters_so_far += 1

            logger.record_tabular("EpisodesSoFar", episodes_so_far)
            logger.record_tabular("TimestepsSoFar", timesteps_so_far)
            logger.record_tabular("TimeElapsed", time.time() - tstart)

            if rank == 0:
                logger.dump_tabular()

def flatten_lists(listoflists):
    return [el for list_ in listoflists for el in list_]
