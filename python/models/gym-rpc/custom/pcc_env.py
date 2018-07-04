import numpy as np
import gym
from gym import spaces
from gym.utils import seeding

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
        self.loss = loss
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

    def __init__(self, model_params):
        self.action_space = spaces.Box(low=np.array([-1e8]), high=np.array([1e8]))
        mins = []
        maxes = []
        for i in range(0, model_params.history_len):
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
