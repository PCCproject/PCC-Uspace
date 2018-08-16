import multiprocessing
import numpy as np
import time
import pickle

class AsyncStash():
    def __init__(self, obj):
        self.queue = multiprocessing.Queue()
        self.queue.put(obj)

    def push(self, obj):
        self.queue.get()
        self.queue.put(obj)

    def pull(self):
        obj = self.queue.get()
        self.queue.put(obj)
        return obj

class Normalizer():
    def __init__(self, sqrt=False):
        self.min = None
        self.max = None
        self.sqrt = sqrt

    def normalize(self, data):
        if self.min is None:
            self.min = np.min(data)

        if self.max is None:
            self.max = np.max(data)

        self.min = min(self.min, np.min(data))
        self.max = max(self.max, np.max(data))

        result = (data - self.min) / (self.max - self.min)
        if self.sqrt:
            result = np.sqrt(result)

        return result

class DataAggregator():

    def __init__(self, flows, replicas, model_params, example_ob, example_ac, norm_rewards=False):
        self.flows = flows
        self.replicas = replicas
        self.flows_per_replica = flows / replicas
        self.flow_size = model_params.ts_per_batch
        self.batch_size = self.flow_size * self.flows
        self.obs = np.array([example_ob for _ in range(self.batch_size)])
        self.h_states = np.zeros([self.batch_size, 32], 'float32')
        self.c_states = np.zeros([self.batch_size, 32], 'float32')
        self.rews = np.zeros(self.batch_size, 'float32')
        self.vpreds = np.zeros(self.batch_size, 'float32')
        self.news = np.zeros(self.batch_size, 'int32')
        self.acs = np.array([example_ac for _ in range(self.batch_size)])
        self.prevacs = self.acs.copy()

        self.norm_rewards = norm_rewards
       
        self.cur_ep_ret = 0
        self.cur_ep_len = 0

        self.ep_rets = []
        self.ep_lens = []
        
        self.cur_flow = 0
        self.cur_replica = 0
        self.next_run_updated = False
        self.next_run = -1
        self.next_released_replica = 0
        self.run_stash = AsyncStash(0)
        self.queue = multiprocessing.Queue()
        self.lock = multiprocessing.Lock()
        
        self.flows_done_training = 0

        self.normalizers = {}

    def give_dataset(self, dataset, block=False):
        dataset = pickle.loads(dataset)
        obs = dataset["ob"]
        h_states = dataset["h_state"]
        c_states = dataset["c_state"]
        rews = dataset["rew"]
        vpreds = dataset["vpred"]
        news = dataset["new"]
        acs = dataset["ac"]
        prevacs = dataset["prevac"]
        self.lock.acquire()
        if self.norm_rewards:
            nonce = dataset["nonce"]
            if nonce not in self.normalizers.keys():
                self.normalizers[nonce] = Normalizer(sqrt=True)
            norm = self.normalizers[nonce]
            rews = norm.normalize(rews)
        this_flow = self.cur_flow
        if (self.next_run == -1):
            self.next_run = 1
        start = self.cur_flow * self.flow_size
        end = start + self.flow_size
        np.copyto(self.obs[start:end], obs)
        np.copyto(self.h_states[start:end], h_states)
        np.copyto(self.c_states[start:end], c_states)
        np.copyto(self.rews[start:end], rews)
        np.copyto(self.vpreds[start:end], vpreds)
        np.copyto(self.news[start:end], news)
        np.copyto(self.acs[start:end], acs)
        np.copyto(self.prevacs[start:end], prevacs)
        self.ep_rets += dataset["ep_rets"]
        self.ep_lens += dataset["ep_lens"]
        self.cur_flow += 1
        if dataset["done_training"]:
            self.flows_done_training += 1
        if (self.cur_flow == self.flows):
            all_done_training = (self.flows_done_training == self.flows)
            self.cur_flow = 0
            dataset = {"ob": self.obs, "h_state":self.h_states, "c_state":self.c_states,
                      "rew": self.rews, "vpred": self.vpreds, "new": self.news,
                      "ac": self.acs, "prevac": self.prevacs, "nextvpred": 0,
                      "ep_rets": self.ep_rets, "ep_lens": self.ep_lens, "stop":all_done_training}
            self.queue.put(dataset)
            self.flows_done_training = 0
            self.ep_rets = []
            self.ep_lens = []
        this_replica = -1
        if block:
            this_replica = self.cur_replica
            self.cur_replica += 1
            if self.cur_replica == self.replicas:
                self.cur_replica = 0
        self.lock.release()
        if (block):
            self.wait_for_new_model(this_replica)

    def wait_for_new_model(self, this_replica):
        print("Simulator " + str(this_replica) + " finished generating data")
        while (self.next_released_replica != this_replica or self.run_stash.pull() < self.next_run):
            time.sleep(0.05)
        self.next_released_replica = this_replica + 1
        if (self.next_released_replica == self.replicas):
            self.next_run += 1
            self.next_released_replica = 0

    def get_dataset(self):
        self.next_run += 1
        self.run_stash.push(self.next_run)
        result = self.queue.get()
        return result
