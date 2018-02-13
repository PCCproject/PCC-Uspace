import numpy as np
from collections import deque
import gym
from gym import spaces
import cv2

def make_pcc(env_id):
    env = gym.make(env_id)
    env = LogEnv(env)
    return env

class LogEnv(gym.Wrapper):
    def __init__(self, env):
        """Wrapping log stuff into the environment
        """
        gym.Wrapper.__init__(self, env)
        print("hey new wrapper!")
        self.actions = env.unwrapped.actions
        self.observations = env.unwrapped.observations
        self.rewards = env.unwrapped.rewards
        # assert env.unwrapped.get_action_meanings()[0] == 'NOOP'

