import numpy as np
from collections import deque
import gym
from gym import spaces
import cv2

def make_pcc(env_id):
    env = gym.make(env_id)
    # env = LogEnv(env)
    env = PCCEnv(env)
    return env


class PCCEnv(gym.Wrapper):
    def __init__(self, env):
        """Wrapping stuff into the environment
        """
        gym.Wrapper.__init__(self, env)
        print("hey new PCC wrapper!")
        self.action_history = env.unwrapped.action_history
        self.observation_history = env.unwrapped.observation_history
        self.reward_history = env.unwrapped.reward_history
        # assert env.unwrapped.get_action_meanings()[0] == 'NOOP'


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

