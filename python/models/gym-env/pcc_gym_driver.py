import multiprocessing


from mpi4py import MPI
from baselines.common import set_global_seeds
import gym
from gym import spaces
from gym.utils import seeding
import numpy as np
import math
import tensorflow as tf

"""
Classic cart-pole system implemented by Rich Sutton et al.
Copied from http://incompleteideas.net/sutton/book/code/pole.c
permalink: https://perma.cc/C9ZM-652R
"""

#
# The monitor interval class used to pass data from the PCC subsystem to
# the machine learning module.
#
class PccMonitorInterval():
    def __init__(self, rate, latency, loss, lat_infl, utility, done):
        self.rate = rate
        self.latency = latency
        self.loss = loss
        self.lat_infl = lat_infl
        self.utility = utility
        self.done = done

    # Convert the observation parts of the monitor interval into a numpy array
    # eb
    def as_array(self):
        return np.array([self.rate, self.latency, self.loss, self.lat_infl])

class PccEnv(gym.Env):
    metadata = {
        'render.modes': ['none'],
    }

    def __init__(self, mi_queue, rate_queue):
        
        self.action_space = spaces.Box(low=np.array([0]), high=np.array([1e9]), dtype=numpy.float32)
        mins = [
        0.0, # Sending rate
        0.0, # Latency
        0.0, # Loss Rate
        -1.0 # Latency Inflation
        ]
        maxes = [
        1e10, # Sending rate
        1e9, # Latency
        1.0, # Loss Rate
        100.0 # Latency Inflation
        ]
        self.observation_space = spaces.Box(low=np.array(mins), high=np.array(maxes), dtype=numpy.float32)
        self.mi_queue = mi_queue
        self.rate_queue = rate_queue
        self.state = np.array(mins)

    def seed(self, seed=None):
        self.np_random, seed = seeding.np_random(seed)
        return [seed]

    def step(self, action):
        action[0] = action[0].astype(np.float32)
        #print("MODEL: action = " + str(action))
        #print(self.action_space)
        assert self.action_space.contains(action)#, "%r (%s) invalid"%(action, type(action))
        rate_queue.put(action)
        mi = mi_queue.get()
        reward = mi.utility
        self.state = mi.as_array()
        return self.state, reward, mi.done, {}

    def reset(self):
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
        return MlpPolicy(name=name, ob_space=env.observation_space, ac_space=env.action_space, hid_size=64,
        num_hid_layers=7) ## Here's the neural network! NHR
    #env = bench.Monitor(env, logger.get_dir() and osp.join(logger.get_dir(), str(rank)))
    env.seed(workerseed)
    #gym.logger.setLevel(logging.WARN)

    # env = wrap_deepmind(env)
    env.seed(workerseed)

    trpo_mpi.learn(env, policy_fn, timesteps_per_batch=512, max_kl=0.001, cg_iters=10, cg_damping=1e-3,
        max_timesteps=int(num_timesteps * 1.1), gamma=0.98, lam=1.0, vf_iters=3, vf_stepsize=1e-4, entcoeff=0.00)
    env.close()

mi_queue = multiprocessing.Queue()
rate_queue = multiprocessing.Queue()

p = multiprocessing.Process(target=train, args=(1e9, 0, mi_queue, rate_queue))
p.start()

def give_sample(sending_rate, latency, loss, lat_infl, utility):
    #print("GIVING SAMPLE")
    mi_queue.put(PccMonitorInterval(
        sending_rate,
        latency,
        loss,
        lat_infl,
        utility,
        False
    ))
    #print("GAVE SAMPLE")

def get_rate():
    #print("GETTING RATE")
    rate = rate_queue.get()[0]
    if rate < 0:
        rate = 0
    elif rate > 1.0:
        rate = 1.0
    rate *= float(1e9)
    #print("\tRATE = " + str(rate / 1000000.0))
    return rate
