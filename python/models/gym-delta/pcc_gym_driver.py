import multiprocessing

import os.path
import time
from mpi4py import MPI
from baselines.common import set_global_seeds
import gym
from gym import spaces
from gym.utils import seeding
import numpy as np
import math
import tensorflow as tf
import sys
import baselines.common.tf_util as U
import random
if not hasattr(sys, 'argv'):
    sys.argv  = ['']

"""
Classic cart-pole system implemented by Rich Sutton et al.
Copied from http://incompleteideas.net/sutton/book/code/pole.c
permalink: https://perma.cc/C9ZM-652R
"""
   
#HISTORY_LEN = 1
HISTORY_LEN = 3
#HID_LAYERS = 1
HID_LAYERS = 3
#HID_SIZE = 1
HID_SIZE = 32
TS_PER_BATCH = 1024
MAX_KL = 0.001
CG_ITERS = 10
CG_DAMPING = 1e-3
GAMMA = 0.0
LAMBDA = 1.0
VF_ITERS = 3
VF_STEPSIZE = 1e-4
ENTCOEFF = 0.00

MIN_RATE = 0.00001
MAX_RATE = 1000.0
DELTA_SCALE = 0.04
    
RATE_SCALE = 1e-9
LATENCY_SCALE = 1e-7
LOSS_SCALE = 1.0
LAT_INFL_SCALE = 1.0
UTILITY_SCALE = 1e-9

LOAD_MODEL = False
SAVE_MODEL = False
MODEL_NAME = "NULL"

MODEL_LOADED = False

RESET_UTILITY_FRACTION = 0.01
RESET_TARGET_RATE_MIN = 20.0
RESET_TARGET_RATE_MAX = 20.0#180.0
RESET_INTERVAL = 200

SAVE_COUNTER = 0
SAVE_INTERVAL = TS_PER_BATCH / 2

for arg in sys.argv:
    arg_val = "NULL"
    try:
        arg_val = float(arg[arg.rfind("=") + 1:])
    except:
        pass

    if "--model-name=" in arg:
        MODEL_NAME = arg[arg.rfind("=") + 1:]

    if "--save-model" in arg:
        SAVE_MODEL = True

    if "--load-model" in arg:
        LOAD_MODEL = True

    if "--delta-rate-scale=" in arg:
        DELTA_SCALE *= arg_val

    if "--all-rate-scale=" in arg:
        MAX_RATE *= arg_val

    if "--hid-layers=" in arg:
        HID_LAYERS = int(arg_val)

    if "--hid-size=" in arg:
        HID_SIZE = int(arg_val)

    if "--ts-per-batch=" in arg:
        TS_PER_BATCH = int(arg_val)

    if "--max-kl=" in arg:
        MAX_KL = arg_val

    if "--cg-iters=" in arg:
        CG_ITERS = int(arg_val)

    if "--cg-damping=" in arg:
        CG_DAMPING = arg_val

    if "--gamma=" in arg:
        GAMMA = arg_val

    if "--lambda=" in arg:
        LAMBDA = arg_val

    if "--vf-iters=" in arg:
        VF_ITERS = int(arg_val)

    if "--vf-stepsize=" in arg:
        VF_STEPSIZE = arg_val

    if "--entcoeff=" in arg:
        ENTCOEFF = arg_val

    if "--history-len=" in arg:
        HISTORY_LEN = int(arg_val)
    
    if "--reset-interval=" in arg:
        RESET_INTERVAL = int(arg_val)

MONITOR_INTERVAL_MIN_OBS = [
    0.0, # Utility
    0.0, # Sending rate
    0.0, # Latency
    0.0, # Loss Rate
    -1.0  # Latency Inflation
]

MONITOR_INTERVAL_MAX_OBS = [
    1.0, # Utility
    1.0, # Sending rate
    1.0, # Latency
    1.0, # Loss Rate
    100.0  # Latency Inflation
]

RESET_COUNTER = 0

STATE_RECORDING_RESET_SAMPLES = "RECORDING_RESET_VALUES"
STATE_RUNNING = "RUNNING"
STATE_RESET = "RESET"

MAX_RESET_SAMPLES = max(HISTORY_LEN, 20)
N_RESET_SAMPLES = 0
RESET_SAMPLES_UTILITY_SUM = 0.0
RESET_RATE_EXPECTED_UTILITY = 0.0

STATE = STATE_RESET

UTIL_EWMA_FACTOR = 0.1
UTIL_EWMA_VAL = 0.0

def update_util_ewma(new_val):
    global UTIL_EWMA_FACTOR
    global UTIL_EWMA_VAL
    UTIL_EWMA_VAL = UTIL_EWMA_FACTOR * new_val + (1.0 - UTIL_EWMA_FACTOR) * UTIL_EWMA_VAL

_last_reset_utility = 0.0

#
# The monitor interval class used to pass data from the PCC subsystem to
# the machine learning module.
#
class PccMonitorInterval():
    def __init__(self, rate=0.0, latency=0.0, loss=0.0, lat_infl=0.0, utility=0.0, done=False, stop=False):
        self.rate = rate
        self.latency = latency
        self.loss = loss / (1.0 - loss)
        self.lat_infl = lat_infl
        self.utility = utility
        self.done = done
        self.stop = stop

        if (stop):
            print("CREATED MONITOR INTERVAL WITH STOP SIGNAL")

        #self.done = True
        
        global RESET_COUNTER
        #print("reset counter = " + str(RESET_COUNTER))
        update_util_ewma(utility)
        #print("UTIL = " + str(UTIL_EWMA_VAL) + " (" + str(RESET_RATE_EXPECTED_UTILITY) + ")")
        if not done and UTIL_EWMA_VAL < RESET_RATE_EXPECTED_UTILITY * RESET_UTILITY_FRACTION:
            self.done = True
            RESET_COUNTER = 0
            #print("RESET -- LOW UTILITY: r: " + str(rate) + " u: " + str(utility) + " (ewma: " + str(UTIL_EWMA_VAL) + ", e: " + str(RESET_RATE_EXPECTED_UTILITY) + " rr: " + str(_last_reset_rate) + ")")
            pass
        
        if RESET_COUNTER >= RESET_INTERVAL:
            self.done = True
            RESET_COUNTER = 0
            #print("\t RESET DUE TO COUNTER (" + str(rate) + ")")
        RESET_COUNTER += 1

    # Convert the observation parts of the monitor interval into a numpy array
    # eb
    def as_array(self):
        #print(np.array([self.utility, self.rate, self.latency, self.loss, self.lat_infl]))
        return np.array([self.utility, self.rate, self.latency, self.loss, self.lat_infl])

class PccHistory():
    def __init__(self, length):
        self.values = []
        for i in range(0, length):
            self.values.append(PccMonitorInterval())

    def step(self, new_mi):
        self.values.pop(0)
        self.values.append(new_mi)

    def as_array(self):
        arrays = []
        for mi in self.values:
            arrays.append(mi.as_array())
        arrays = np.array(arrays).flatten()
        return arrays

sess = "NULL"
def save_model(model_name):
    global sess
    saver = tf.train.Saver()
    saver.save(sess, model_name)
    my_vars = tf.global_variables()
    for v in my_vars:
        print(v.name + " = " + str(sess.run(v)))

def load_model(model_name):
    if os.path.isfile(model_name + ".meta"):
        saver = tf.train.Saver()
        saver.restore(sess, model_name)
    #my_vars = tf.global_variables()
    #for v in my_vars:
    #    print(v.name)
    #    if v.name == "pi/obfilter/runningsum:0":
    #        val = sess.run(v)
    #        print(val)
    #print(my_vars)
    #exit(0)

class PccEnv(gym.Env):
    metadata = {
        'render.modes': ['none'],
    }

    def __init__(self, mi_queue, rate_queue):
        global HISTORY_LEN
        self.hist = PccHistory(HISTORY_LEN)
        self.state = self.hist.as_array()
        self.action_space = spaces.Box(low=np.array([-1e6]), high=np.array([1e6]))
        mins = []
        maxes = []
        for i in range(0, HISTORY_LEN):
            mins += MONITOR_INTERVAL_MIN_OBS
            maxes += MONITOR_INTERVAL_MAX_OBS
        self.observation_space = spaces.Box(low=np.array(mins), high=np.array(maxes))
        self.mi_queue = mi_queue
        self.rate_queue = rate_queue
        self.was_reset = False

    def seed(self, seed=None):
        self.np_random, seed = seeding.np_random(seed)
        return [seed]

    def step(self, action):
        should_reset = self.was_reset
        self.was_reset = False
        action[0] = action[0].astype(np.float32)
        if not self.action_space.contains(action):
            print("ERROR: Action space does not contain value: " + action)
            exit(-1)
        #print("Putting rate on rate queue")
        rate_queue.put((action, should_reset))
        #print("Waiting for MI from PCC...")
        #print("Getting MI from queue")
        mi = mi_queue.get()
        #print("GOT MONITOR INTERVAL")
        global SAVE_MODEL
        global MODEL_NAME
        if mi.stop:
            #print("\tWITH STOP SIGNAL")
            if SAVE_MODEL:
                print("SAVING MODEL")
                save_model(MODEL_NAME)
            print("EXITING")
            exit(0)
        
        if SAVE_MODEL:
            global SAVE_COUNTER
            global SAVE_INTERVAL
            SAVE_COUNTER += 1
            if SAVE_COUNTER == SAVE_INTERVAL:
                SAVE_COUNTER = 0
                save_model(MODEL_NAME)
            
        
        self.hist.step(mi)
        reward = mi.utility# * 1e-8
        self.state = self.hist.as_array()
        #print(self.state)
        #print("Returning reward: " + str(reward) + " for action " + str(action))
        #exit(0)
        return self.state, reward, mi.done, {}

    def reset(self):
        #print("RESET CALLED!")
        self.was_reset = True
        #exit(0)
        global LOAD_MODEL
        global MODEL_LOADED
        global MODEL_NAME
        if LOAD_MODEL and not MODEL_LOADED:
            MODEL_LOADED = True
            load_model(MODEL_NAME)
        global HISTORY_LEN
        self.hist = PccHistory(HISTORY_LEN)
        self.state = self.hist.as_array()
        return self.state

    def render(self, mode='none'):
        return None

    def close(self):
        pass
    
def train(num_timesteps, seed, mi_queue, rate_queue):
    from policies.basic_nn import BasicNNPolicy
    #from policies.nosharing_cnn_policy import CnnPolicy
    from policies.simple_cnn import CnnPolicy
    from policies.mlp_policy import MlpPolicy
    from policies.simple_policy import SimplePolicy
    from baselines_master.trpo_mpi import trpo_mpi
    global sess
    rank = MPI.COMM_WORLD.Get_rank()
    sess = U.single_threaded_session()
    sess.__enter__()
    #if rank == 0:
    #    logger.configure()
    #else:
    #    logger.configure(format_strs=[])

    workerseed = seed + 10000 * MPI.COMM_WORLD.Get_rank()
    set_global_seeds(workerseed)
    env = PccEnv(mi_queue, rate_queue) # Need to be changed from Atari, so we need to register with Gym and get the if NHR

    # def policy_fn(name, ob_space, ac_space): #pylint: disable=W0613
    #     return BasicNNPolicy(name=name, ob_space=env.observation_space, ac_space=env.action_space) ## Here's the neural network! NHR
    def policy_fn(name, ob_space, ac_space): #pylint: disable=W0613
        return MlpPolicy(
            name=name,
            ob_space=env.observation_space,
            ac_space=env.action_space,
            hid_size=HID_SIZE,
            num_hid_layers=HID_LAYERS,
            gaussian_fixed_var=False
        )
    #env = bench.Monitor(env, logger.get_dir() and osp.join(logger.get_dir(), str(rank)))
    env.seed(workerseed)
    #gym.logger.setLevel(logging.WARN)

    #env = wrap_deepmind(env)
    env.seed(workerseed)

    global TS_PER_BATCH
    global MAX_KL
    global CG_ITERS
    global CG_DAMPING
    global GAMMA
    global LAMBDA
    global VF_ITERS
    global VF_STEPSIZE
    global ENTCOEFF
    trpo_mpi.learn(env, policy_fn, 
        timesteps_per_batch=TS_PER_BATCH,
        max_kl=MAX_KL,
        cg_iters=CG_ITERS,
        cg_damping=CG_DAMPING,
        max_timesteps=num_timesteps,
        gamma=GAMMA,
        lam=LAMBDA,
        vf_iters=VF_ITERS,
        vf_stepsize=VF_STEPSIZE,
        entcoeff=ENTCOEFF
    )
    env.close()

mi_queue = multiprocessing.Queue()
rate_queue = multiprocessing.Queue()

p = multiprocessing.Process(target=train, args=(1e9, 0, mi_queue, rate_queue))
p.start()
_prev_rate_delta = 0.0
_gave_trailing_sample = False
_first_sample = True

def give_sample(sending_rate, latency, loss, lat_infl, utility, stop=False):
    
    global _first_sample
    global STATE
    global STATE_RESET
    if _first_sample:
        _first_sample = False
        if STATE == STATE_RESET:
            return

    sending_rate *= RATE_SCALE
    latency *= LATENCY_SCALE
    loss *= LOSS_SCALE
    lat_infl *= LAT_INFL_SCALE
    utility *= UTILITY_SCALE

    #print("GIVING SAMPLE: " + STATE)
    global STATE_RECORDING_RESET_SAMPLES
    global STATE_RUNNING
    global MAX_RESET_SAMPLES
    global N_RESET_SAMPLES
    global RESET_SAMPLES_UTILITY_SUM
    global RESET_RATE_EXPECTED_UTILITY
    global UTIL_EWMA_VAL
    global _gave_trailing_sample

    if stop:
        mi_queue.put(PccMonitorInterval(
            sending_rate, latency, loss,
            lat_infl, utility, False, stop))

    elif STATE == STATE_RUNNING:
        #print("Putting MI on queue.")
        mi_queue.put(PccMonitorInterval(
            sending_rate,
            latency,
            loss,
            lat_infl,
            utility,
            False,
            stop
        ))
    elif STATE == STATE_RESET:
        if _gave_trailing_sample:
            #print("Sample: r: " + str(_last_reset_rate) + " u: " + str(utility))
            RESET_SAMPLES_UTILITY_SUM += utility
            N_RESET_SAMPLES += 1
            RESET_RATE_EXPECTED_UTILITY = utility#RESET_SAMPLES_UTILITY_SUM / float(N_RESET_SAMPLES)
            UTIL_EWMA_VAL = RESET_RATE_EXPECTED_UTILITY
            if N_RESET_SAMPLES == MAX_RESET_SAMPLES:
                N_RESET_SAMPLES = 0
                RESET_SAMPLES_UTILITY_SUM = 0.0
                STATE = STATE_RUNNING
        else:
            #print("Putting MI on queue.")
            mi_queue.put(PccMonitorInterval(
                sending_rate,
                latency,
                loss,
                lat_infl,
                utility,
                False,
                stop
            ))
            _gave_trailing_sample = True
    
    elif STATE == STATE_RECORDING_RESET_SAMPLES:
        #print("Sample: r: " + str(_last_reset_rate) + " u: " + str(utility))
        RESET_SAMPLES_UTILITY_SUM += utility
        N_RESET_SAMPLES += 1
        RESET_RATE_EXPECTED_UTILITY = utility#RESET_SAMPLES_UTILITY_SUM / float(N_RESET_SAMPLES)
        UTIL_EWMA_VAL = RESET_RATE_EXPECTED_UTILITY
        if N_RESET_SAMPLES == MAX_RESET_SAMPLES:
            N_RESET_SAMPLES = 0
            RESET_SAMPLES_UTILITY_SUM = 0.0
            STATE = STATE_RUNNING

_prev_rate = None

def apply_rate_delta(rate, rate_delta):
    global MIN_RATE
    global MAX_RATE

    # We want a string of actions with average 0 to result in a rate change
    # of 0, so delta of 0.05 means rate * 1.05,
    # delta of -0.05 means rate / 1.05
    if rate_delta > 0:
        rate *= (1.0 + rate_delta)
    elif rate_delta < 0:
        rate /= (1.0 - rate_delta)
    
    # For practical purposes, we may have maximum and minimum rates allowed.
    if rate < MIN_RATE:
        rate = MIN_RATE
    if rate > MAX_RATE:
        rate = MAX_RATE

    return rate
    

def get_rate():
    global STATE
    #print("GETTING RATE: " + STATE)
    
    global _prev_rate
    global _last_reset_rate
    global _gave_trailing_sample
    global MAX_RESET_SAMPLES
    global N_RESET_SAMPLES
    
    global RESET_TARGET_RATE_MIN
    global RESET_TARGET_RATE_MAX
    
    if STATE == STATE_RUNNING:
        rate_delta, reset = rate_queue.get()
        rate_delta = rate_delta[0] * DELTA_SCALE
        
        rate = apply_rate_delta(_prev_rate, rate_delta)

        if reset:
            STATE = STATE_RESET
            _gave_trailing_sample = False

    elif STATE == STATE_RESET:
        rate = random.uniform(RESET_TARGET_RATE_MIN, RESET_TARGET_RATE_MAX)
        _last_reset_rate = rate * 1e6
        STATE = STATE_RECORDING_RESET_SAMPLES

    elif STATE == STATE_RECORDING_RESET_SAMPLES:
        rate = _prev_rate

    _prev_rate = rate
    return rate * 1e6
