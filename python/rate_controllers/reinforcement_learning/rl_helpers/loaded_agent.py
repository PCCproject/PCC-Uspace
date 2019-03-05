from rl_helpers import pcc_event_log
import tensorflow as tf
import math
import numpy as np
import time
import sys
import os
import io

import pickle

class LoadedModel():

    def __init__(self, model_path):
        self.sess = tf.Session()
        self.model_path = model_path
        self.metagraph = tf.saved_model.loader.load(self.sess,
            [tf.saved_model.tag_constants.SERVING], self.model_path)
        sig = self.metagraph.signature_def["serving_default"]
        input_dict = dict(sig.inputs)
        output_dict = dict(sig.outputs)       
 
        self.input_obs_label = input_dict["ob"].name
        self.input_state_label = None
        self.initial_state = None
        self.state = None
        if "state" in input_dict.keys():
            self.input_state_label = input_dict["state"].name
            strfile = io.StringIO()
            print(input_dict["state"].tensor_shape, file=strfile)
            lines = strfile.getvalue().split("\n")
            #print(lines)
            dim_1 = int(lines[1].split(":")[1].strip(" "))
            dim_2 = int(lines[4].split(":")[1].strip(" "))
            self.initial_state = np.zeros((dim_1, dim_2), dtype=np.float32)
            self.state = np.zeros((dim_1, dim_2), dtype=np.float32)
 
        self.output_act_label = output_dict["act"].name
        self.output_stochastic_act_label = None
        if "stochastic_act" in output_dict.keys():
            self.output_stochastic_act_label = output_dict["stochastic_act"].name

        self.mask = None
        self.input_mask_label = None 
        if "mask" in input_dict.keys():
            self.input_mask_label = input_dict["mask"].name
            self.mask = np.ones((1, 1)).reshape((1, ))

    def reset_state(self):      
        self.state = np.copy(self.initial_state)

    def reload(self):
        self.metagraph = tf.saved_model.loader.load(self.sess,
            [tf.saved_model.tag_constants.SERVING], self.model_path)
 
    def act(self, obs, stochastic=False):
        input_dict = {self.input_obs_label:obs}
        if self.state is not None:
            input_dict[self.input_state_label] = self.state

        if self.mask is not None:
            input_dict[self.input_mask_label] = self.mask

        sess_output = None
        if stochastic:
            sess_output = self.sess.run(self.output_stochastic_act_label, feed_dict=input_dict)
        else:
            sess_output = self.sess.run(self.output_act_label, feed_dict=input_dict)

        action = None
        if len(sess_output) > 1:
            action, self.state = sess_output
        else:
            action = sess_output

        return {"act":action}


##
#   A TrpoAgent object receives observations, takes actions, and is given rewards. It records
#   this information in a TrpoDataset. When it has a complete dataset, it saves it to a file and
#   waits for an update to its model. The TrpoAgent then loads the new model and begins the
#   cycle again.
##
class LoadedModelAgent():

    def __init__(self, flow_id, network_id, model_path, stochastic=False, log=None, nonce=None, pause_on_full=False):
        self.model = LoadedModel(model_path)

        self.next_action_id = 0
        
        self.model_timestamp = None
        self.stochastic = stochastic
        self.log = log

        self.actions = {}

        self.flow_id = flow_id
        self.network_id = network_id
        self.nonce = nonce
        # TODO: Probably unwise to hard-code the data directory. We can't train multiple models
        # on the same machine with this method.
        self.data_filename = "/tmp/pcc_rl_data/flow_%d_nonce_%d.dat" % (flow_id, self.nonce)

        self.use_scale_free_obs = True
        self.min_latency = None
        self.pause_on_full = pause_on_full

    def _load_model(self):
        self.model.reload()

    def give_reward(self, action_id, action, reward):
        if (action_id < 0):
            return

        if (self.actions[action_id] != action):
            print("ERROR: Mismatch in actions and IDs")
            exit(-1)
        """
        self.dataset.record_reward(action_id, reward)
        if self.dataset.finished():
            self.finish_epoch()
        #"""

    def finish_epoch(self):
        """
        if not (self.log is None):
            self.log.log_event({"Name":"Training Epoch", "Episode Reward":self.dataset.avg_reward()})
            self.log.flush()

        self.dataset.scale_vpred(self.vpred_scale)
        data_dict = self.dataset.as_dict()
        pickle.dump(data_dict, open(self.data_filename, "wb"))
        self.first_epoch = False
        #"""

    def start_new_epoch(self):
        #self.dataset.reset()
        #self.load_model()
        self.next_action_id = 0

    def reset(self):
        action_id = self.next_action_id
        #self.dataset.reset_near_action(action_id)
        #self.model.reset_state()

    def make_scale_free_ob(self, ob):
        history_len = int(len(ob) / 6)
        #print("Ob length: %d, history length: %d" % (len(ob), history_len))
        scale_free_ob = np.zeros((3 * history_len,))
        #print("Made scale_free_ob")
        for i in range(0, history_len):
            #print("Creating scale_free_ob %d" % i)
            send_rate = ob[6 * i + 1]
            #print("Send rate done")
            thpt = ob[6 * i + 2]
            #print("Throughput done")
            lat = ob[6 * i + 3]
            #print("Latency done")
            lat_infl = ob[6 * i + 5]
            #print("Latency inflation done")
            scale_free_ob[3 * i + 0] = lat_infl
            #print("Latency inflation assigned, self.min_latency = %s, lat = %s" % (self.min_latency, lat))
            if lat > 0.0 and ((self.min_latency is None) or (lat < self.min_latency)):
                self.min_latency = lat
            if self.min_latency is not None:
                scale_free_ob[3 * i + 1] = lat / self.min_latency
            else:
                scale_free_ob[3 * i + 1] = 1.0
            #print("Latency ratio assignedi, send_rate = %s, thtp = %s" % (send_rate, thpt))
            if send_rate == 0.0 and thpt == 0.0:
                scale_free_ob[3 * i + 2] = 1.0
            elif send_rate > 1000.0 * thpt:
                scale_free_ob[3 * i + 2] = 1000.0
            else:
                scale_free_ob[3 * i + 2] = send_rate / thpt
            #print("Send ratio assigned")
        #print("Returning scale_free_ob")
        return scale_free_ob

    def act(self, ob):

        #print(ob[:5], file=sys.stderr)
        act_dict = None
        #print("Ob = %s" % str(ob))
        #print("Taking action!")
        if self.use_scale_free_obs:
            scale_free_ob = self.make_scale_free_ob(ob)
            #print("Scale-free ob: %s" % str(scale_free_ob))
            act_dict = self.model.act(scale_free_ob.reshape((1, int(len(ob) / 2))), stochastic=False)#self.stochastic)
        else:
            act_dict = self.model.act(ob[:5].reshape((1, 5)), stochastic=True)#self.stochastic)

        ac = act_dict["act"]
        vpred = act_dict["vpred"] if "vpred" in act_dict.keys() else None
        state = act_dict["state"] if "state" in act_dict.keys() else None
        action_id = -1

        """
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
        elif (self.dataset.finished() and self.pause_on_full):
            while (self.model_timestamp == os.stat(self.model_name + ".meta").st_mtime):
                time.sleep(1.0)

        self.prevac = ac
        #"""

        #print("Action: %s" % str(ac))
        return action_id, ac

