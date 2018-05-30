from custom import trpo_agent
from custom import pcc_env
from custom import model_param_set
from custom import data_aggregator
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


from xmlrpc.server import SimpleXMLRPCServer
from xmlrpc.server import SimpleXMLRPCRequestHandler
import socketserver

if not hasattr(sys, 'argv'):
    sys.argv  = ['']

MODEL_PATH = "/tmp/"
MODEL_NAME = "pcc_model_" + str(int(round(time.time() * 1000)))

TRAINING_CLIENTS = 1

for arg in sys.argv:
    arg_val = "NULL"
    try:
        arg_val = float(arg[arg.rfind("=") + 1:])
    except:
        pass

    if "--ml-training-clients=" in arg:
        TRAINING_CLIENTS = int(arg_val)

    if "--model-name=" in arg:
        MODEL_NAME = arg[arg.rfind("=") + 1:]
    
    if "--model-path=" in arg:
        MODEL_PATH = arg[arg.rfind("=") + 1:]
   
model_params = model_param_set.ModelParameterSet(MODEL_NAME, MODEL_PATH)
env = pcc_env.PccEnv(model_params)
stoc = True
if "--deterministic" in sys.argv:
    stoc = False
data_agg = data_aggregator.DataAggregator(TRAINING_CLIENTS, model_params, env.observation_space.sample(), env.action_space.sample())

def policy_fn(name, ob_space, ac_space): #pylint: disable=W0613
    return MlpPolicy(
        name=name,
        ob_space=env.observation_space,
        ac_space=env.action_space,
        hid_size=model_params.hidden_size,
        num_hid_layers=model_params.hidden_layers,
        gaussian_fixed_var=True
    )

def train(data_agg, env, policy_fn):
    sess = U.single_threaded_session()
    sess.__enter__()
    trainer = trpo_mpi.TrpoTrainer(data_agg, None, env, policy_fn, 
        timesteps_per_batch=model_params.ts_per_batch,
        max_kl=model_params.max_kl,
        cg_iters=model_params.cg_iters,
        cg_damping=model_params.cg_damping,
        max_timesteps=1e9,
        gamma=model_params.gamma,
        lam=model_params.lam,
        vf_iters=model_params.vf_iters,
        vf_stepsize=model_params.vf_stepsize,
        entcoeff=model_params.entcoeff
    )
    trainer.train(model_params.path + model_params.name)

p = multiprocessing.Process(target=train, args=[data_agg, env, policy_fn])
p.start()

"""
sess = U.single_threaded_session()
sess.__enter__()
trainer = trpo_mpi.TrpoTrainer(agent, env, policy_fn, 
    timesteps_per_batch=model_params.ts_per_batch,
    max_kl=model_params.max_kl,
    cg_iters=model_params.cg_iters,
    cg_damping=model_params.cg_damping,
    max_timesteps=1e9,
    gamma=model_params.gamma,
    lam=model_params.lambda,
    vf_iters=model_params.vf_iters,
    vf_stepsize=model_params.vf_stepsize,
    entcoeff=model_params.entcoeff
)
"""

def give_dataset(dataset):
    data_agg.give_dataset(dataset)
    return 0

# Restrict to a particular path.
class RequestHandler(SimpleXMLRPCRequestHandler):
    rpc_paths = ('/RPC2',)

class RPCThreading(socketserver.ThreadingMixIn, SimpleXMLRPCServer):
    pass

# Create server
server = RPCThreading(("localhost", 8000), requestHandler=RequestHandler)
#server = SimpleXMLRPCServer(("localhost", 8000), requestHandler=RequestHandler)
server.register_introspection_functions()

# Register pow() function; this will use the value of
# pow.__name__ as the name, which is just 'pow'.
server.register_function(give_dataset)

# Run the server's main loop
server.serve_forever()
