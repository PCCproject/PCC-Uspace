from rl_helpers import pcc_event_log
import tensorflow as tf
import math
import numpy as np
import time
import sys
import os

import pickle

##
#   A single dataset intended to be used with the OpenAI TRPO implementation. Each training
#   process has a single one of these datasets. All flows contribute data until the dataset is
#   full. It's then written to a file, and a DataAggregator will pass the data to a TRPO training
#   algorithm.
##
class TrpoDataset():
    def __init__(self, flow_id, nonce, batch_size, example_ob, example_ac):
        self.flow_id = flow_id
        self.nonce = nonce

        self.batch_size = batch_size
        self.obs = np.array([example_ob for _ in range(batch_size)])
        self.h_state = np.zeros([batch_size, 1], 'float32')
        self.c_state = np.zeros([batch_size, 1], 'float32')
        self.rews = np.zeros(batch_size, 'float32')
        self.vpreds = np.zeros(batch_size, 'float32')
        self.news = np.zeros(batch_size, 'int32')
        self.acs = np.array([example_ac for _ in range(batch_size)])
        self.prevacs = self.acs.copy()
       
        self.cur_ep_ret = 0
        self.cur_ep_len = 0

        self.ep_rets = []
        self.ep_lens = []

        self.n_actions = 0
        self.n_rewards = 0

        self.resets = []
        self.term_vpred = 0
        self.term_new = 0

        self.epoch_rews = []
        self.epochs_since_best = 0
        self.best_mean_epoch_rew = None
        self.vpred_scale = 1.0

    def reset(self):
        self.n_actions = 0
        self.n_rewards = 0
        self.news = np.zeros(self.batch_size, 'int32')
        self.news[0] = 1
        self.ep_lens = []
        self.ep_rets = []
        self.cur_ep_len = 0
        self.cur_ep_ret = 0
        self.resets = []
        self.term_vpred = 0

    def get_batch_size(self):
        return self.batch_size

    def full(self):
        return self.n_actions == self.batch_size

    def finished(self):
        return self.n_rewards == self.batch_size

    def reset_if_in_dataset(self, action_id):
        if (action_id >= 0 and action_id < self.batch_size):
            self.news[action_id] = 1

    def reset_near_action(self, action_id):
        if (len(self.resets) > 0 and self.resets[-1] == action_id):
            return
        # We may not want to reset so many places near an action, but we do because it's not
        # clear exaclty which action was the one that needed to be reset.
        self.reset_if_in_dataset(action_id - 2)
        self.reset_if_in_dataset(action_id - 1)
        self.reset_if_in_dataset(action_id + 0)
        self.reset_if_in_dataset(action_id + 1)
        self.ep_rets.append(self.cur_ep_ret)
        self.ep_lens.append(self.cur_ep_len)
        self.cur_ep_ret = 0
        self.cur_ep_len = 0
        self.resets.append(action_id)

    def scale_vpred(self, vpred_scale):
        self.vpred_scale = vpred_scale

    def is_rew_stable(self):
        # This was intended to be a way to see if reward had stabilized and training should stop.
        # Right now, the duration is set so long that it will never be triggered, but the code
        # remains in, just in case we want it later.
        dur = 3000

        if len(self.epoch_rews) < dur:
            return False

        cur_mean_rew = sum(self.epoch_rews[-1 * dur:]) / dur
        if (self.best_mean_epoch_rew is None or cur_mean_rew > self.best_mean_epoch_rew):
            print("Improved (%s -> %f)" % (str(self.best_mean_epoch_rew), cur_mean_rew))
            self.best_mean_epoch_rew = cur_mean_rew
            self.epochs_since_best = 0
        else:
            self.epochs_since_best += 1
            print("No improvement (%d/%d)" % (self.epochs_since_best, dur))

        if (self.best_mean_epoch_rew is not None and self.epochs_since_best > dur):
            return True

        return False

    def as_dict(self):
        stop_training = self.is_rew_stable()
        result = {"ob": self.obs, "h_state":self.h_state, "c_state":self.c_state, 
                  "rew": self.rews, "vpred": self.vpreds, "new": self.news,
                  "ac": self.acs, "prevac": self.prevacs, "nextvpred": 0,
                  "ep_rets": self.ep_rets, "ep_lens": self.ep_lens,
                  "done_training":stop_training, "flow_id":self.flow_id, "nonce":self.nonce,
                  "vpred_scale":self.vpred_scale}
        return result

    def avg_reward(self):
        if len(self.ep_rets) == 0:
            return 0
        return np.sum(self.ep_rets) / np.sum(self.ep_lens)

    def record_reward(self, action_id, reward):
        # Record a single reward. We need to know which action is being rewarded. It may not just
        # be the most recent action. You might take several actions before receiving the reward.

        self.rews[action_id] = reward
        self.n_rewards += 1
        self.cur_ep_ret += reward
        self.cur_ep_len += 1
        if self.finished():
            self.epoch_rews.append(np.mean(self.rews))

    def record_action(self, action_id, ob, vpred, ac, prevac, h_state=None, c_state=None):
        # Record a single action taken. To train on this later, we need to know the observations,
        # previous action, predicted value, and any state. Once we get a reward for this action,
        # we will also have to record that info.

        i = action_id
        self.obs[i] = ob
        self.vpreds[i] = vpred
        self.acs[i] = ac
        self.prevacs[i] = prevac
        self.n_actions += 1

        if (h_state is not None):
            self.h_state[i] = h_state

        if (c_state is not None):
            self.c_state[i] = c_state

    def record_terminal_vpred(self, vpred):
        self.term_vpred = vpred

##
#   A TrpoAgent object receives observations, takes actions, and is given rewards. It records
#   this information in a TrpoDataset. When it has a complete dataset, it saves it to a file and
#   waits for an update to its model. The TrpoAgent then loads the new model and begins the
#   cycle again.
##
class TrpoAgent():

    def __init__(self, env, flow_id, network_id, model_name, model_params, model, stochastic=True, log=None, nonce=None):
        self.model_name = model_name
        self.model_loaded = False

        self.next_action_id = 0
        
        self.model = model
        self.model_timestamp = None
        self.stochastic = stochastic
        self.prevac = None

        self.log = log

        self.actions = {}

        self.flow_id = flow_id
        self.network_id = network_id
        self.nonce = nonce
        self.dataset = TrpoDataset(flow_id, nonce, model_params.ts_per_batch,
            env.observation_space.sample(), env.action_space.sample())
        self.load_model()
        # TODO: Probably unwise to hard-code the data directory. We can't train multiple models
        # on the same machine with this method.
        self.data_filename = "/tmp/pcc_rl_data/flow_%d_nonce_%d.dat" % (flow_id, self.nonce)
        self.vpred_scale = 1.0
        self.first_epoch = True

    def load_model(self):
        if os.path.isfile(self.model_name + ".meta"):
            saver = tf.train.Saver()
            saver.restore(tf.get_default_session(), self.model_name)
            self.model_timestamp = os.stat(self.model_name + ".meta").st_mtime
        else:
            print("ERROR: Could not load model " + self.model_name)
            exit(-1)

    def give_reward(self, action_id, action, reward):
        if (action_id < 0):
            return

        if self.first_epoch and abs(reward) / self.vpred_scale > 10.0:
            # TODO: Consider some means of initial scaling of rewards?
            #self.vpred_scale = math.pow(10.0, int(round(math.log10(abs(reward)))))
            pass

        if (self.actions[action_id] != action):
            print("ERROR: Mismatch in actions and IDs")
            exit(-1)
        self.dataset.record_reward(action_id, reward)
        if self.dataset.finished():
            self.finish_epoch()

    def finish_epoch(self):
        if not (self.log is None):
            self.log.log_event({"Name":"Training Epoch", "Episode Reward":self.dataset.avg_reward()})
            self.log.flush()

        self.dataset.scale_vpred(self.vpred_scale)
        data_dict = self.dataset.as_dict()
        pickle.dump(data_dict, open(self.data_filename, "wb"))
        self.first_epoch = False

    def start_new_epoch(self):
        self.dataset.reset()
        self.load_model()
        self.next_action_id = 0

    def reset(self):
        action_id = self.next_action_id
        self.dataset.reset_near_action(action_id)
        self.model.reset_state()

    def act(self, ob):

        ac, vpred, h_state, c_state = self.model.act(self.stochastic, ob)
        action_id = -1

        new_model_timestamp = os.stat(self.model_name + ".meta").st_mtime
        if self.model_timestamp is None or self.model_timestamp < new_model_timestamp:
            self.start_new_epoch()

        if (not self.dataset.full()):
            action_id = self.next_action_id
            self.dataset.record_action(action_id, ob, vpred, ac, self.prevac,
                h_state=h_state, c_state=c_state)
            self.prevac = ac
            self.next_action_id += 1
            self.actions[action_id] = ac
        else:
            pass

        self.prevac = ac

        return action_id, ac

