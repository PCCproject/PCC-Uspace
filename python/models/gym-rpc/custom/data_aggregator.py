#import threading
import multiprocessing
import numpy as np
import time

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

class DataAggregator():

    def __init__(self, replicas, model_params, example_ob, example_ac, norm_rewards=False):
        self.replicas = replicas
        self.replica_size = model_params.ts_per_batch
        self.batch_size = self.replica_size * replicas
        self.obs = np.array([example_ob for _ in range(self.batch_size)])
        self.rews = np.zeros(self.batch_size, 'float32')
        self.vpreds = np.zeros(self.batch_size, 'float32')
        self.news = np.zeros(self.batch_size, 'int32')
        self.acs = np.array([example_ac for _ in range(self.batch_size)])
        self.prevacs = self.acs.copy()
       
        self.cur_ep_ret = 0
        self.cur_ep_len = 0

        self.ep_rets = []
        self.ep_lens = []
        
        self.cur_replica = 0
        self.next_run_updated = False
        self.next_run = -1
        self.next_released_replica = 0
        self.run_stash = AsyncStash(0)
        self.queue = multiprocessing.Queue()
        self.lock = multiprocessing.Lock()
        #self.lock = threading.Lock()

    def give_dataset(self, dataset):
        obs = np.array(dataset["ob"])
        rews = np.array(dataset["rew"])
        vpreds = np.array(dataset["vpred"])
        news = np.array(dataset["new"])
        acs = np.array(dataset["ac"])
        prevacs = np.array(dataset["prevac"])
        if (norm_rewards):
            min_rew = np.min(rews)
            max_rew = np.max(rews)
            rews = (rews - min_rew) / (max_rew - min_rew)
        self.lock.acquire()
        this_replica = self.cur_replica
        if (self.next_run == -1):
            self.next_run = 1
        start = self.cur_replica * self.replica_size
        end = start + self.replica_size
        np.copyto(self.obs[start:end], obs)
        np.copyto(self.rews[start:end], rews)
        np.copyto(self.vpreds[start:end], vpreds)
        np.copyto(self.news[start:end], news)
        np.copyto(self.acs[start:end], acs)
        np.copyto(self.prevacs[start:end], prevacs)
        self.ep_rets += dataset["ep_rets"]
        self.ep_lens += dataset["ep_lens"]
        self.cur_replica += 1
        if (self.cur_replica == self.replicas):
            self.cur_replica = 0
            dataset = {"ob": self.obs, "rew": self.rews, "vpred": self.vpreds, "new": self.news,
                      "ac": self.acs, "prevac": self.prevacs, "nextvpred": 0,
                      "ep_rets": self.ep_rets, "ep_lens": self.ep_lens}
            self.queue.put(dataset)
            self.ep_rets = []
            self.ep_lens = []
        self.lock.release()
        self.wait_for_new_model(this_replica)

    def wait_for_new_model(self, this_replica):
        print("Simulator " + str(this_replica) + " finished generating data")
        while (not (self.next_released_replica >= this_replica and self.run_stash.pull() >= self.next_run)):
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
