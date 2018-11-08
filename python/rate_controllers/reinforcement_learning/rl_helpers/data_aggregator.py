import multiprocessing
import numpy as np
import time
import pickle
import os

##
#   This class is used to normalize input utility to be closer to the range [-10, 10]. Right now
#   this is using fixed values that work for the usual training cases because the adaptive
#   normalization was causing the model to learn to fool the normalizer instead of learning a
#   better policy.
##
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

        # TODO: Consider other forms of normalization. Replace fixed values.
        self.min = 4.7e7 - 1e8
        data_min = np.min(data)
        if (data_min < self.min):
            print("data lower: data_min = %f, self.min = %f" % (data_min, self.min))
        self.max = 4.8e7
        data_max = np.max(data)
        if (data_max > self.max):
            print("data higher: data_max = %f, self.max = %f" % (data_max, self.max))

        result = (data - self.min) / (self.max - self.min)
        if self.sqrt:
            result = np.sqrt(result)

        return result

##
#   This object keeps track of a data file that will be updated from one of our training
#   replicas. A single DataAggregator reads from many data files to get a full dataset for our
#   model.
##
class DataFile():

    def __init__(self, filename, is_stale=False):
        self.filename = filename
        self.finished = False
        self.timestamp = None
        if is_stale:
            self.os.stat(filename).st_mtime

    def is_finished(self):
        return self.finished

    def has_new_data(self):
        if self.timestamp is None:
            return True
        new_time = os.stat(self.filename).st_mtime
        return new_time > self.timestamp

    def get_data(self):
        data = pickle.load(open(self.filename, "rb"))
        self.timestamp = os.stat(self.filename).st_mtime
        return data

##
#   A DataAggregator keeps track of several DataFile objects and determines when they have new
#   data that can be given to the training algorithm.
##
class DataAggregator():

    def __init__(self, model_params, n_flows, data_dir, example_ob, example_ac, norm_rewards):
        self.n_flows = n_flows
        self.n_flows_finished = 0
        self.data_dir = data_dir
        self.flow_size = model_params.ts_per_batch

        self.example_ob = example_ob
        self.example_ac = example_ac

        self.batch_size = None
        self.obs = None 
        self.h_states = None 
        self.c_states = None 
        self.rews = None 
        self.vpreds = None 
        self.vpreds_unscaled = None 
        self.news = None 
        self.acs = None 
        self.prevacs = None 

        self.norm_rewards = norm_rewards
       
        self.cur_ep_ret = 0
        self.cur_ep_len = 0

        self.ep_rets = []
        self.ep_lens = []

        self.normalizers = {}
        self.data_files = []
        self.contributor_dict = {}
        self.setup_dataset()

    def setup_dataset(self):
        self.batch_size = self.flow_size * self.n_flows
        self.obs = np.array([self.example_ob for _ in range(self.batch_size)])
        self.h_states = np.zeros([self.batch_size, 1], 'float32')
        self.c_states = np.zeros([self.batch_size, 1], 'float32')
        self.rews = np.zeros(self.batch_size, 'float32')
        self.vpreds = np.zeros(self.batch_size, 'float32')
        self.vpred_scales = np.zeros(self.batch_size, 'float32')
        self.news = np.zeros(self.batch_size, 'int32')
        self.acs = np.array([self.example_ac for _ in range(self.batch_size)])
        self.prevacs = self.acs.copy()

    def check_for_new_contributors(self):
        # We are checking a directory for any new data files. If we find any new files, we assume
        # that they are a part of our training run.
        
        obj_names = os.listdir(self.data_dir)
        filenames = []
        for obj_name in obj_names:
            if os.path.isfile(os.path.join(self.data_dir, obj_name)):
                filenames.append(os.path.join(self.data_dir, obj_name))
        
        for filename in filenames:
            if filename not in self.contributor_dict.keys():
                self.data_files.append(DataFile(filename))
                self.contributor_dict[filename] = True

    def dataset_as_dict(self):

        # This is the dictionary that training algorithms like TRPO will use to update the model.
        return {"ob": self.obs, "h_state":self.h_states,
            "c_state":self.c_states, "rew": self.rews, "vpred": self.vpreds,
            "vpred_scale":self.vpred_scales,
            "new": self.news, "ac": self.acs, "prevac": self.prevacs,
            "nextvpred": 0, "ep_rets": self.ep_rets, "ep_lens": self.ep_lens,
            "stop":False}

    def all_data_files_finished(self):
        if len(self.data_files) < self.n_flows:
            return False
        for f in self.data_files:
            if not f.is_finished():
                return False
        return True

    def get_new_data_from_file(self, data_file):
        # When a file has new data, we need to load it in as a dictionary, normalize it, and copy
        # the data to our buffers.

        dataset = data_file.get_data()
        if self.norm_rewards:
            nonce = dataset["nonce"]
            if nonce not in self.normalizers.keys():
                self.normalizers[nonce] = Normalizer(sqrt=False)
            norm = self.normalizers[nonce]
            dataset["rew"] = norm.normalize(dataset["rew"])
        start = self.n_flows_finished * self.flow_size
        end = start + self.flow_size
        np.copyto(self.obs[start:end], dataset["ob"])
        np.copyto(self.h_states[start:end], dataset["h_state"])
        np.copyto(self.c_states[start:end], dataset["c_state"])
        np.copyto(self.rews[start:end], dataset["rew"])
        np.copyto(self.vpreds[start:end], dataset["vpred"] * dataset["vpred_scale"])
        np.copyto(self.vpred_scales[start:end], dataset["vpred_scale"])
        np.copyto(self.news[start:end], dataset["new"])
        np.copyto(self.acs[start:end], dataset["ac"])
        np.copyto(self.prevacs[start:end], dataset["prevac"])
        self.ep_rets += dataset["ep_rets"]
        self.ep_lens += dataset["ep_lens"]
        self.n_flows_finished += 1

    def check_files_for_new_data(self):
        for f in self.data_files:
            if (not f.is_finished()) and f.has_new_data():
                print("File %s updated" % f.filename)
                try:
                    self.get_new_data_from_file(f)
                    f.finished = True
                except EOFError as e:
                    print(e)

    def reset_data_file_status(self):
        for f in self.data_files:
            f.finished = False
        self.n_flows_finished = 0

    def poll_for_data(self):
        # This is an infinite loop that checks the data directory for data from new training
        # agents, or new data from existing agents. It then yields this data to the training
        # algorithm.
        
        while True:
            if self.n_flows > len(self.data_files):
                self.check_for_new_contributors()
            self.check_files_for_new_data()
            if self.all_data_files_finished():
                self.reset_data_file_status()
                yield self.dataset_as_dict()
            time.sleep(2.0)
