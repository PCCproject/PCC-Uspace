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

    def __init__(self, replicas, model_params, example_ob, example_ac):
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
        self.lock.acquire()
        this_replica = self.cur_replica
        if (self.next_run == -1):
            self.next_run = 1
        start = self.cur_replica * self.replica_size
        end = start + self.replica_size
        np.copyto(self.obs[start:end], np.array(dataset["ob"]))
        np.copyto(self.rews[start:end], np.array(dataset["rew"]))
        np.copyto(self.vpreds[start:end], np.array(dataset["vpred"]))
        np.copyto(self.news[start:end], np.array(dataset["new"]))
        np.copyto(self.acs[start:end], np.array(dataset["ac"]))
        np.copyto(self.prevacs[start:end], np.array(dataset["prevac"]))
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
        print("waiting for new model " + str(this_replica))
        while (not (self.next_released_replica >= this_replica and self.run_stash.pull() >= self.next_run)):
            time.sleep(0.05)
        self.next_released_replica = this_replica + 1
        if (self.next_released_replica == self.replicas):
            self.next_run += 1
            self.next_released_replica = 0
        print("finished waiting for new model " + str(this_replica))

    def get_dataset(self):
        self.next_run += 1
        self.run_stash.push(self.next_run)
        result = self.queue.get()
        return result
