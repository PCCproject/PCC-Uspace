
print("Beginning python module import...")

from rl_helpers import model_param_set
from rl_helpers import pcc_env
from rl_helpers import loaded_agent
from rl_helpers import pcc_event_log
from rl_helpers.simple_arg_parse import arg_or_default
from collections import deque
import os.path
import time
import sys
import open_ai.common.tf_util as U
import random
import xmlrpc.client
    
from policies.lstm_policy import LstmPolicy
from policies.mlp_policy import MlpPolicy
from open_ai import trpo

if not hasattr(sys, 'argv'):
    sys.argv  = ['']

MIN_RATE = 0.5
MAX_RATE = 300.0
#DELTA_SCALE = 0.005
DELTA_SCALE = 0.05

RESET_RATE_MIN = 5.0
RESET_RATE_MAX = 100.0

RESET_RATE_MIN = 6.0
RESET_RATE_MAX = 6.0

RESET_INTERVAL = 1024

MODEL_PATH = arg_or_default("--model-path", "/tmp/")

LOG_NAME = None
PORT = 8000

NONCE = None

for arg in sys.argv:
    arg_str = "NULL"
    try:
        arg_str = arg[arg.rfind("=") + 1:]
    except:
        pass

    if "--nonce=" in arg:
        NONCE = int(arg_str)

    if "--model-path=" in arg:
        MODEL_PATH = arg_str

    if "--reset-target-rate=" in arg:
        RESET_RATE_MIN = float(arg_str)
        RESET_RATE_MAX = float(arg_str)

    if "--delta-rate-scale=" in arg:
        DELTA_SCALE *= float(arg_str)

    if "--no-reset" in arg:
        RESET_INTERVAL = 1e9

    if "--ml-log=" in arg:
        LOG_NAME = arg_str

    if "--ml-port=" in arg:
        PORT = int(arg_str)

model_params = model_param_set.ModelParameterSet("", MODEL_PATH)

stoc = False
if "--deterministic" in sys.argv:
    stoc = False

log = None
if LOG_NAME is not None:
    log = pcc_event_log.PccEventLog(LOG_NAME, nonce=NONCE)

class PccGymDriver():
    
    flow_lookup = {}
    
    def __init__(self, flow_id):
        global RESET_RATE_MIN
        global RESET_RATE_MAX
        global HISTORY_LEN

        self.reset_counter = 0

        self.waiting_action_ids = deque([])
        self.current_rate = random.uniform(RESET_RATE_MIN, RESET_RATE_MAX)
        self.history = pcc_env.PccHistory(model_params.history_len)
        self.actions = {}
        self.got_data = False

        self.agent = loaded_agent.LoadedModelAgent(
            flow_id,
            0,
            MODEL_PATH,
            stochastic=stoc,
            log=log,
            nonce=NONCE,
            pause_on_full=("--ml-pause-during-training" in sys.argv)
        )

        PccGymDriver.flow_lookup[flow_id] = self

    def get_next_waiting_action_id(self):
        if len(self.waiting_action_ids) == 0:
            #print("ERROR: Attempted to return data for an action for which there was no ID")
            return -1
        return self.waiting_action_ids.popleft()

    def push_waiting_action_id(self, action_id, action):
        self.waiting_action_ids.append(action_id)
        self.actions[action_id] = action

    def get_current_rate(self):
        return self.current_rate

    def get_rate(self):
        global RESET_INTERVAL
        prev_rate = self.get_current_rate()
        if self.has_data():
            action_id, rate_delta = self.agent.act(self.get_current_observation())
            self.push_waiting_action_id(action_id, rate_delta)
            rate = apply_rate_delta(prev_rate, rate_delta)
            self.reset_counter += 1
            if (self.reset_counter >= RESET_INTERVAL):
                self.reset()
                rate = self.get_current_rate()
        
            self.set_current_rate(rate)
            return float(rate * 1e6)

        print("Got rate! (staying same %f)" % self.get_current_rate())
        return self.get_current_rate() * 1e6

    def has_data(self):
        return self.got_data

    def set_current_rate(self, new_rate):
        self.current_rate = new_rate

    def record_observation(self, new_mi):
        self.history.step(new_mi)
        self.got_data = True

    def reset_rate(self):
        self.current_rate = random.uniform(RESET_RATE_MIN, RESET_RATE_MAX)

    def reset_history(self):
        self.history = pcc_env.PccHistory(model_params.history_len)
        self.got_data = False

    def reset(self):
        self.agent.reset()
        self.reset_rate()
        self.reset_history()
        self.reset_counter = 0

    def get_current_observation(self):
        return self.history.as_array()

    def get_action(self, action_id):
        if action_id < 0:
            return None
        return self.actions[action_id]

    def give_sample(self, sending_rate, recv_rate, latency, loss, lat_infl, utility, stop):
        self.record_observation(
            pcc_env.PccMonitorInterval(
                sending_rate / (8.0 * 1500.0 * 1000.0 * 8.0),
                recv_rate / (8.0 * 1500.0 * 1000.0 * 8.0),
                latency / (10.0 * 1000.0 * 1000.0),
                loss,
                lat_infl,
                utility
            )
        )
        action_id = self.get_next_waiting_action_id()
        self.agent.give_reward(action_id, self.get_action(action_id), utility)

    def get_by_flow_id(flow_id):
        return PccGymDriver.flow_lookup[flow_id]

def give_sample(flow_id, sending_rate, recv_rate, latency, loss, lat_infl, utility, stop=False):
    driver = PccGymDriver.get_by_flow_id(flow_id)
    driver.give_sample(sending_rate, recv_rate, latency, loss, lat_infl, utility, stop)
    #driver.give_sample(sending_rate, recv_rate, latency, loss, lat_infl, utility / float(1e9), stop)

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
    
def reset(flow_id):
    driver = PccGymDriver.get_by_flow_id(flow_id)
    driver.reset()

def get_rate(flow_id):
    #print("Getting rate")
    driver = PccGymDriver.get_by_flow_id(flow_id)
    return driver.get_rate()

def init(flow_id):
    driver = PccGymDriver(flow_id)
