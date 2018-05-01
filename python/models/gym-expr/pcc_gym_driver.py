from custom import trpo_agent
import multiprocessing
from collections import deque
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
    
from policies.basic_nn import BasicNNPolicy
#from policies.nosharing_cnn_policy import CnnPolicy
from policies.simple_cnn import CnnPolicy
from policies.mlp_policy import MlpPolicy
from policies.simple_policy import SimplePolicy
from baselines_master.trpo_mpi import trpo_mpi

if not hasattr(sys, 'argv'):
    sys.argv  = ['']

HISTORY_LEN = 3
HID_LAYERS = 3
HID_SIZE = 32
TS_PER_BATCH = 4096
MAX_KL = 0.0001
CG_ITERS = 10
CG_DAMPING = 1e-3
GAMMA = 0.0
LAMBDA = 1.0
VF_ITERS = 3
VF_STEPSIZE = 1e-4
ENTCOEFF = 0.00

MIN_RATE = 2.0
MAX_RATE = 500.0
#DELTA_SCALE = 0.01
DELTA_SCALE = 0.04

RESET_RATE_MIN = 5.0
RESET_RATE_MAX = 100.0

RESET_COUNTER = 0
RESET_INTERVAL = 1200

MODEL_NAME = "/tmp/pcc_model_" + str(int(round(time.time() * 1000)))

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

    if "--reset-target-rate=" in arg:
        RESET_RATE_MIN = arg_val
        RESET_RATE_MAX = arg_val

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
    
    if "--no-reset" in arg:
        RESET_INTERVAL = 1e9

MONITOR_INTERVAL_MIN_OBS = [
    0.0, # Utility
    0.0, # Sending rate
    0.0, # Recv rate
    0.0, # Latency
    0.0, # Loss Rate
    -1.0  # Latency Inflation
]

MONITOR_INTERVAL_MAX_OBS = [
    1.0, # Utility
    1.0, # Sending rate
    1.0, # Recv rate
    1.0, # Latency
    100.0, # Loss Rate
    100.0  # Latency Inflation
]

#
# The monitor interval class used to pass data from the PCC subsystem to
# the machine learning module.
#
class PccMonitorInterval():
    def __init__(self, rate=0.0, recv_rate=0.0, latency=0.0, loss=0.0, lat_infl=0.0, utility=0.0):
        self.rate = rate
        self.recv_rate = recv_rate
        self.latency = latency
        self.loss = 1.0 - loss
        self.lat_infl = lat_infl
        self.utility = utility

    # Convert the observation parts of the monitor interval into a numpy array
    def as_array(self):
        return np.array([self.utility / 1e9, self.rate / 1e9, self.recv_rate / 1e9, self.latency / 1e6, self.loss, self.lat_infl])

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

class PccEnv(gym.Env):
    metadata = {
        'render.modes': ['none'],
    }

    def __init__(self):
        self.action_space = spaces.Box(low=np.array([-1e8]), high=np.array([1e8]))
        mins = []
        maxes = []
        for i in range(0, HISTORY_LEN):
            mins += MONITOR_INTERVAL_MIN_OBS
            maxes += MONITOR_INTERVAL_MAX_OBS
        self.observation_space = spaces.Box(low=np.array(mins), high=np.array(maxes))

    def seed(self, seed=None):
        self.np_random, seed = seeding.np_random(seed)
        return [seed]

    def step(self, action):
        print("ERROR: Environment should never have 'step()' called -- the agent object should make all actions and receive all rewards.")
        exit(-1)

    def reset(self):
        print("ERROR: Environment should never be reset -- reset using the agent object instead.")
        exit(-1)

    def render(self, mode='none'):
        return None

    def close(self):
        pass
   
env = PccEnv()
stoc = True
if "--deterministic" in sys.argv:
    stoc = False
agent = trpo_agent.TrpoAgent(model_name=MODEL_NAME, stochastic=stoc)

def policy_fn(name, ob_space, ac_space): #pylint: disable=W0613
    return MlpPolicy(
        name=name,
        ob_space=env.observation_space,
        ac_space=env.action_space,
        hid_size=HID_SIZE,
        num_hid_layers=HID_LAYERS,
        gaussian_fixed_var=True
    )



def train(agent, env, policy_fn):
    sess = U.single_threaded_session()
    sess.__enter__()
    trainer = trpo_mpi.TrpoTrainer(agent, env, policy_fn, 
        timesteps_per_batch=TS_PER_BATCH,
        max_kl=MAX_KL,
        cg_iters=CG_ITERS,
        cg_damping=CG_DAMPING,
        max_timesteps=1e9,
        gamma=GAMMA,
        lam=LAMBDA,
        vf_iters=VF_ITERS,
        vf_stepsize=VF_STEPSIZE,
        entcoeff=ENTCOEFF
    )
    trainer.train(agent, MODEL_NAME)

p = multiprocessing.Process(target=train, args=[agent, env, policy_fn])
p.start()
    
sess = U.single_threaded_session()
sess.__enter__()
trainer = trpo_mpi.TrpoTrainer(agent, env, policy_fn, 
    timesteps_per_batch=TS_PER_BATCH,
    max_kl=MAX_KL,
    cg_iters=CG_ITERS,
    cg_damping=CG_DAMPING,
    max_timesteps=1e9,
    gamma=GAMMA,
    lam=LAMBDA,
    vf_iters=VF_ITERS,
    vf_stepsize=VF_STEPSIZE,
    entcoeff=ENTCOEFF
)

class PccGymDriver():
    def __init__(self):
        global RESET_RATE_MIN
        global RESET_RATE_MAX
        global HISTORY_LEN

        self.waiting_action_ids = deque([])
        self.current_rate = random.uniform(RESET_RATE_MIN, RESET_RATE_MAX)
        self.history = PccHistory(HISTORY_LEN)
        self.actions = {}

    def get_next_waiting_action_id(self):
        if len(self.waiting_action_ids) == 0:
            print("ERROR: Attempted to return data for an action for which there was no ID")
            return -1
        return self.waiting_action_ids.popleft()

    def push_waiting_action_id(self, action_id, action):
        self.waiting_action_ids.append(action_id)
        self.actions[action_id] = action

    def get_current_rate(self):
        return self.current_rate

    def set_current_rate(self, new_rate):
        self.current_rate = new_rate

    def record_observation(self, new_mi):
        self.history.step(new_mi)

    def reset_rate(self):
        self.current_rate = random.uniform(RESET_RATE_MIN, RESET_RATE_MAX)

    def reset_history(self):
        self.history = PccHistory(HISTORY_LEN)

    def get_current_observation(self):
        return self.history.as_array()

    def get_action(self, action_id):
        return self.actions[action_id]

driver = PccGymDriver()

def give_sample(sending_rate, recv_rate, latency, loss, lat_infl, utility, stop=False):
    global driver
    #print("Give sample")
    driver.record_observation(
        PccMonitorInterval(
            sending_rate,
            recv_rate,
            latency,
            loss,
            lat_infl,
            utility
        )
    )
    if (latency == 0):
        #print("Give sample \\")
        return
    #print("Give Sample")
    action_id = driver.get_next_waiting_action_id()
    #print("Give reward " + str(utility) + ", id = " + str(action_id))
    agent.give_reward(action_id, driver.get_action(action_id), utility)
    #print("Give sample \\")

def apply_rate_delta(rate, rate_delta):
    global MIN_RATE
    global MAX_RATE
    global DELTA_SCALE
    
    rate_delta *= DELTA_SCALE

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
    
def reset():
    global agent
    global RESET_COUNTER
    #print("pcc_gym_driver.py: reset()")
    agent.reset()
    driver.reset_rate()
    driver.reset_history()
    RESET_COUNTER = 0

def get_rate():
    global driver
    global RESET_COUNTER
    global RESET_INTERVAL
    #print("Get rate")
    prev_rate = driver.get_current_rate()
    #print("Python: get_rate() prev_rate = " + str(prev_rate))
    action_id, rate_delta = agent.act(driver.get_current_observation())
    #print("Python: get_rate() rate_delta = " + str(rate_delta) + ", id = " + str(action_id))
    driver.push_waiting_action_id(action_id, rate_delta)
    rate = apply_rate_delta(prev_rate, rate_delta)
    #print("Python: get_rate() new_rate = " + str(rate))
    RESET_COUNTER += 1
    if (RESET_COUNTER >= RESET_INTERVAL):
        #print("pcc_gym_driver.py: calling reset() due to reset counter")
        reset()
        rate = driver.get_current_rate()
        #print("Python: get_rate() reset, new_rate = " + str(rate))
    driver.set_current_rate(rate)
    #print("Python: get_rate() returned rate = " + str(rate * 1e6))
    #print("Rate: " + str(rate * 1e6))
    return float(rate * 1e6)
