from custom import trpo_agent
from custom import pcc_env
from custom import model_param_set
from custom import data_aggregator
import multiprocessing
import os.path
import numpy as np
import sys
import baselines.common.tf_util as U
    
from policies.mlp_policy import MlpPolicy
from baselines_master.trpo_mpi import trpo_mpi

from xmlrpc.server import SimpleXMLRPCServer
from xmlrpc.server import SimpleXMLRPCRequestHandler
import socketserver

import tensorflow as tf

if not hasattr(sys, 'argv'):
    sys.argv  = ['']

MODEL_PATH = "/tmp/"
MODEL_NAME = "cur_model"
        
MODEL_CHECKPOINT_FREQ = 0
MODEL_CHECKPOINT_DIR = None

MAX_ITERS = 1e9
PORT = 8000

TRAINING_CLIENTS = 1
TRAINING_FLOWS = 1

should_load_model = False

for arg in sys.argv:
    arg_val = "NULL"
    try:
        arg_val = float(arg[arg.rfind("=") + 1:])
    except:
        pass

    if "--ml-port=" in arg:
        PORT = int(arg_val)

    if "--ml-training-flows=" in arg:
        TRAINING_FLOWS = int(arg_val)

    if "--ml-training-clients=" in arg:
        TRAINING_CLIENTS = int(arg_val)

    if "--model-name=" in arg:
        MODEL_NAME = arg[arg.rfind("=") + 1:]
    
    if "--model-path=" in arg:
        MODEL_PATH = arg[arg.rfind("=") + 1:]
    
    if "--ml-max-iters=" in arg:
        MAX_ITERS = int(arg_val)
    
    if "--ml-cp-freq=" in arg:
        MODEL_CHECKPOINT_FREQ = int(arg_val)
    
    if "--ml-cp-dir=" in arg:
        MODEL_CHECKPOINT_DIR = arg[arg.rfind("=") + 1:]

    if arg == "--load-model":
        should_load_model = True

def load_model():
    if os.path.isfile(MODEL_PATH + MODEL_NAME + ".meta"):
        saver = tf.train.Saver()
        saver.restore(tf.get_default_session(), MODEL_PATH + MODEL_NAME)
    else:
        print("ERROR: Could not load model " + MODEL_PATH + MODEL_NAME)
        exit(-1)

if TRAINING_FLOWS < TRAINING_CLIENTS:
    TRAINING_FLOWS = TRAINING_CLIENTS

model_params = model_param_set.ModelParameterSet(MODEL_NAME, MODEL_PATH)
env = pcc_env.PccEnv(model_params)
stoc = True
if "--deterministic" in sys.argv:
    stoc = False
data_agg = data_aggregator.DataAggregator(TRAINING_FLOWS, TRAINING_CLIENTS, model_params,
env.observation_space.sample(), env.action_space.sample(), norm_rewards=True)

def policy_fn(name, ob_space, ac_space): #pylint: disable=W0613
    return MlpPolicy(
        name=name,
        ob_space=env.observation_space,
        ac_space=env.action_space,
        hid_size=model_params.hidden_size,
        num_hid_layers=model_params.hidden_layers,
        gaussian_fixed_var=True
    )

def train(data_agg, env, policy_fn, finished_queue):
    sess = U.single_threaded_session()
    sess.__enter__()
    trainer = trpo_mpi.TrpoTrainer(data_agg, env, policy_fn, 
        max_kl=model_params.max_kl,
        cg_iters=model_params.cg_iters,
        cg_damping=model_params.cg_damping,
        max_timesteps=0,
        max_iters=MAX_ITERS,
        gamma=model_params.gamma,
        lam=model_params.lam,
        vf_iters=model_params.vf_iters,
        vf_stepsize=model_params.vf_stepsize,
        entcoeff=model_params.entcoeff,
        checkpoint_freq=MODEL_CHECKPOINT_FREQ,
        checkpoint_dir=MODEL_CHECKPOINT_DIR
    )
    if (should_load_model):
        load_model()
    trainer.train(model_params.path + model_params.name)
    print("Shutting down server")
    finished_queue.put(1)

# Restrict to a particular path.
class RequestHandler(SimpleXMLRPCRequestHandler):
    rpc_paths = ('/RPC2',)

class RPCThreading(socketserver.ThreadingMixIn, SimpleXMLRPCServer):
    pass

# Create server
server = RPCThreading(("localhost", PORT), requestHandler=RequestHandler, logRequests=False)
server.timeout = 1
finished_queue = multiprocessing.Queue()

p = multiprocessing.Process(target=train, args=[data_agg, env, policy_fn, finished_queue])
p.start()

def give_dataset(dataset, block):
    data_agg.give_dataset(dataset, block)
    return 0

server.register_introspection_functions()

server.register_function(give_dataset)

# Run the server's main loop
done = False
while not done:
    server.handle_request()
    done = not finished_queue.empty()

print("Joining child process")
p.join()

print("Server finished serving")
server.server_close()

print("Server closed")
exit(-1)
