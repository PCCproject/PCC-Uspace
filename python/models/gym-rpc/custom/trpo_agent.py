from custom import pcc_event_log
import tensorflow as tf
import numpy as np
import time
import sys
import os

class TrpoDataset():
    def __init__(self, batch_size, example_ob, example_ac):
        self.batch_size = batch_size
        self.obs = np.array([example_ob for _ in range(batch_size)])
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
            self.news[action_id] = 0

    def reset_near_action(self, action_id):
        if (len(self.resets) > 0 and self.resets[-1] == action_id):
            return
        self.reset_if_in_dataset(action_id + 1)
        self.ep_rets.append(self.cur_ep_ret)
        self.ep_lens.append(self.cur_ep_len)
        self.cur_ep_ret = 0
        self.cur_ep_len = 0
        self.resets.append(action_id)

    def is_rew_stable(self):
        dur = 200

        if len(self.epoch_rews) < 2 * dur:
            return False

        first_mean = sum(self.epoch_rews[-2 * dur:-1 * dur]) / dur
        second_mean = sum(self.epoch_rews[-1 * dur:]) / dur

        stable = (abs(2.0 * (first_mean - second_mean) / (first_mean + second_mean)) < 0.01)

        if stable:
            print("Reward stable: %f, %f" % (first_mean, second_mean))
        else:
            print("Reward unstable: %f, %f" % (first_mean, second_mean))
        return stable

    def as_dict(self):
        stop_training = False
        if self.is_rew_stable():
            stop_training = True
        result = {"ob": self.obs.tolist(), "rew": self.rews.tolist(), "vpred": self.vpreds.tolist(), "new": self.news.tolist(),
                  "ac": self.acs.tolist(), "prevac": self.prevacs.tolist(), "nextvpred": 0,
                  "ep_rets": self.ep_rets, "ep_lens": self.ep_lens, "done_training":stop_training}
        return result

    def avg_reward(self):
        if len(self.ep_rets) == 0:
            return 0
        return np.sum(self.ep_rets) / np.sum(self.ep_lens)

    def record_reward(self, action_id, reward):
        self.rews[action_id] = reward
        self.n_rewards += 1
        self.cur_ep_ret += reward
        self.cur_ep_len += 1
        if self.finished():
            self.epoch_rews.append(np.mean(self.rews))

    def record_action(self, action_id, ob, vpred, ac, prevac):
        i = action_id
        self.obs[i] = ob
        self.vpreds[i] = vpred
        self.acs[i] = ac
        self.prevacs[i] = prevac
        self.n_actions += 1

    def record_terminal_vpred(self, vpred):
        self.term_vpred = vpred

class TrpoAgent():
    def __init__(self, server, model_name, stochastic=True, log=None):
        self.model_name = model_name
        self.model_loaded = False
        self.server = server

        self.next_action_id = 0
        
        self.dataset = None
        self.model = None
        self.stochastic = stochastic
        self.prevac = None

        self.log = log

        self.actions = {}

    def init(self, model, batch_size, example_ob, example_ac):
        self.dataset = TrpoDataset(batch_size, example_ob, example_ac)
        self.model = model

    def load_model(self):
        if os.path.isfile(self.model_name + ".meta"):
            saver = tf.train.Saver()
            saver.restore(tf.get_default_session(), self.model_name)
        else:
            print("ERROR: Could not load model " + self.model_name)
            exit(-1)
      
        """
        if self.server is None:
            my_vars = tf.global_variables()
            for v in my_vars:
                if not "oldpi" in v.name:
                    val = tf.get_default_session().run(v)
                    print(v.name + ", max = " + str(np.max(val)) + ", min = " + str(np.min(val)))
        """

    def give_reward(self, action_id, action, reward):
        if (action_id >= 0):
            if (self.actions[action_id] != action):
                print("ERROR: Mismatch in actions and IDs")
                exit(-1)
            self.dataset.record_reward(action_id, reward)
            if (self.dataset.finished()):
                if not (self.log is None):
                    self.log.log_event({"Name":"Training Epoch", "Episode Reward":self.dataset.avg_reward()})
                    self.log.flush()
                data_dict = self.dataset.as_dict()
                if self.server is not None:
                    self.server.give_dataset(data_dict)
                self.dataset.reset()
                self.next_action_id = 0
                self.load_model()

    def reset(self):
        action_id = self.next_action_id
        self.dataset.reset_near_action(action_id)

    def act(self, ob):

        if (not self.model_loaded):
            self.load_model()
            self.model_loaded = True
        ac, vpred = self.model.act(self.stochastic, ob)
        action_id = -1

        if (not self.dataset.full()):
            action_id = self.next_action_id
            self.dataset.record_action(action_id, ob, vpred, ac, self.prevac)
            self.prevac = ac
            self.next_action_id += 1
            self.actions[action_id] = ac
        else:
            pass

        self.prevac = ac

        return action_id, ac

