import multiprocessing

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
if not hasattr(sys, 'argv'):
    sys.argv  = ['']

"""
Classic cart-pole system implemented by Rich Sutton et al.
Copied from http://incompleteideas.net/sutton/book/code/pole.c
permalink: https://perma.cc/C9ZM-652R
"""
   
HISTORY_LEN = 10
HID_LAYERS = 3
HID_SIZE = 64
TS_PER_BATCH = 512
MAX_KL = 0.001
CG_ITERS = 10
CG_DAMPING = 1e-3
GAMMA = 0.9
LAMBDA = 1.0
VF_ITERS = 3
VF_STEPSIZE = 1e-4
ENTCOEFF = 0.00

MIN_RATE = 0.00001
MAX_RATE = 1000.0
DELTA_SCALE = 0.2

for arg in sys.argv:
    arg_val = "NULL"
    if "=" in arg and "log=" not in arg:
        arg_val = float(arg[arg.rfind("=") + 1:])

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

RESET_UTILITY_FRACTION = 0.1
RESET_RATE_TARGET = 2.0

STATE_RECORDING_RESET_SAMPLES = "RECORDING_RESET_VALUES"
STATE_RUNNING = "RUNNING"

MAX_RESET_SAMPLES = 4
N_RESET_SAMPLES = 0
RESET_SAMPLES_UTILITY_SUM = 0.0
RESET_RATE_EXPECTED_UTILITY = 0.0

STATE = STATE_RECORDING_RESET_SAMPLES

UTIL_EWMA_FACTOR = 0.66
UTIL_EWMA_VAL = 0.0

def update_util_ewma(new_val):
    global UTIL_EWMA_FACTOR
    global UTIL_EWMA_VAL
    UTIL_EWMA_VAL = UTIL_EWMA_FACTOR * new_val + (1.0 - UTIL_EWMA_FACTOR) * UTIL_EWMA_VAL

#
# The monitor interval class used to pass data from the PCC subsystem to
# the machine learning module.
#
class PccMonitorInterval():
    def __init__(self, rate=0.0, latency=0.0, loss=0.0, lat_infl=0.0, utility=0.0, done=False):
        self.rate = rate
        self.latency = latency
        self.loss = loss
        self.lat_infl = lat_infl
        self.utility = utility
        self.done = done

        update_util_ewma(utility)
        print("UTIL = " + str(UTIL_EWMA_VAL) + " (" + str(RESET_RATE_EXPECTED_UTILITY) + ")")
        if UTIL_EWMA_VAL < RESET_RATE_EXPECTED_UTILITY * RESET_UTILITY_FRACTION:
            self.done = True

    # Convert the observation parts of the monitor interval into a numpy array
    # eb
    def as_array(self):
        return np.array([self.utility * 1e-8, self.rate * 1e-8, self.latency * 1e-6, self.loss, self.lat_infl])

class PccHistory():
    def __init__(self, length):
        self.values = []
        for i in range(0, length):
            self.values.append(PccMonitorInterval())

    def step(self, new_mi):
        self.values.pop()
        self.values.append(new_mi)

    def as_array(self):
        arrays = []
        for mi in self.values:
            arrays.append(mi.as_array())
        arrays = np.array(arrays).flatten()
        return arrays

class PccEnv(gym.Env):
    metadata = {
        'render.modes': ['none'],
    }

    def __init__(self, mi_queue, rate_queue):
        global HISTORY_LEN
        self.hist = PccHistory(HISTORY_LEN)
        self.state = self.hist.as_array()
        self.action_space = spaces.Box(low=np.array([-100.0]), high=np.array([100.0]))
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
        rate_queue.put((action, should_reset))
        #print("Waiting for MI from PCC...")
        mi = mi_queue.get()
        self.hist.step(mi)
        reward = mi.utility
        self.state = self.hist.as_array()
        return self.state, reward, mi.done, {}

    def reset(self):
        print("RESET CALLED!")
        self.was_reset = True
        #exit(0)
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
    import baselines.common.tf_util as U
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
        #return BasicNNPolicy(name=name, ob_space=env.observation_space, ac_space=env.action_space) ## Here's the neural network! NHR
        #return CnnPolicy(name=name, ob_space=env.observation_space, ac_space=env.action_space) ## Here's the neural network! NHR
        #return SimplePolicy(name=name, ob_space=env.observation_space, ac_space=env.action_space, hid_size=64, num_hid_layers=7) ## Here's the neural network! NHR
        return MlpPolicy(name=name, ob_space=env.observation_space, ac_space=env.action_space, hid_size=HID_SIZE, num_hid_layers=HID_LAYERS) ## Here's the neural network! NHR
    #env = bench.Monitor(env, logger.get_dir() and osp.join(logger.get_dir(), str(rank)))
    env.seed(workerseed)
    #gym.logger.setLevel(logging.WARN)

    # env = wrap_deepmind(env)
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

p = multiprocessing.Process(target=train, args=(1e9, int(time.time()), mi_queue, rate_queue))
p.start()

def give_sample(sending_rate, latency, loss, lat_infl, utility):
    #print("GIVING SAMPLE")
    global STATE
    global STATE_RECORDING_RESET_SAMPLES
    global STATE_RUNNING
    global MAX_RESET_SAMPLES
    global N_RESET_SAMPLES
    global RESET_SAMPLES_UTILITY_SUM
    global RESET_RATE_EXPECTED_UTILITY
    global UTIL_EWMA_VAL

    if STATE == STATE_RECORDING_RESET_SAMPLES:
        print("GIVE SAMPLE: RECORDING RESET SAMPLES = " + str(utility))
        RESET_SAMPLES_UTILITY_SUM += utility
        N_RESET_SAMPLES += 1
        #print("N_RESET_SAMPLES = " + str(N_RESET_SAMPLES))
        RESET_RATE_EXPECTED_UTILITY = RESET_SAMPLES_UTILITY_SUM / float(N_RESET_SAMPLES)
        UTIL_EWMA_VAL = 0.0
        #if N_RESET_SAMPLES == MAX_RESET_SAMPLES:
        #    N_RESET_SAMPLES = 0
        #    STATE = STATE_RUNNING
    elif STATE == STATE_RUNNING:
        #print("GIVE SAMPLE: RUNNING")
        mi_queue.put(PccMonitorInterval(
            sending_rate,
            latency,
            loss,
            lat_infl,
            utility,
            False
        ))

_prev_rate = RESET_RATE_TARGET

def get_rate():
    #print("GETTING RATE")
    # Rate normally varies from -5 to 5
    global _prev_rate
    global RESET_RATE_TARGET
    global STATE
    global STATE_RECORDING_RESET_SAMPLES
    global STATE_RUNNING
    global MAX_RESET_SAMPLES
    global N_RESET_SAMPLES
    global RESET_SAMPLES_UTILITY_SUM
    #print("Rate delta: " + str(rate_delta))
    if STATE == STATE_RUNNING:
        #print("Waiting for rate from ML...")
        rate_delta, reset = rate_queue.get()
        rate_delta = rate_delta[0] * DELTA_SCALE
        if reset:
            _prev_rate = RESET_RATE_TARGET
            STATE = STATE_RECORDING_RESET_SAMPLES
            return _prev_rate * 1e6
        rate = _prev_rate
        rate *= (1.0 + rate_delta)
        if rate < MIN_RATE:
            rate = MIN_RATE
        if rate > MAX_RATE:
            rate = MAX_RATE
        _prev_rate = rate
        return rate * 1e6
    elif STATE == STATE_RECORDING_RESET_SAMPLES:
        if N_RESET_SAMPLES == MAX_RESET_SAMPLES:
            N_RESET_SAMPLES = 0
            RESET_SAMPLES_UTILITY_SUM = 0.0
            STATE = STATE_RUNNING
        return RESET_RATE_TARGET * 1e6

