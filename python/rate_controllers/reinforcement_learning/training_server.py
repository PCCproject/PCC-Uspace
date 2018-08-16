from rl_helpers import trpo_agent
from rl_helpers import pcc_env
from rl_helpers import model_param_set
from rl_helpers import data_aggregator
from rl_helpers.simple_arg_parse import arg_or_default
import multiprocessing
import os.path
import numpy as np
import sys
import open_ai.common.tf_util as U

from policies.lstm_policy import LstmPolicy
from policies.mlp_policy import MlpPolicy
from open_ai import trpo

from xmlrpc.server import SimpleXMLRPCServer
from xmlrpc.server import SimpleXMLRPCRequestHandler
import socketserver

import tensorflow as tf

if not hasattr(sys, 'argv'):
    sys.argv  = ['']

MODEL_PATH = arg_or_default("--model-path", "/tmp/")
MODEL_NAME = arg_or_default("--model-name", "model")
        
MODEL_CHECKPOINT_FREQ = int(arg_or_default("--ml-cp-freq", default=0))
MODEL_CHECKPOINT_DIR = arg_or_default("--ml-cp-dir", default=None)

MAX_ITERS = int(arg_or_default("--ml-max-iters", default=1e9))
PORT = int(arg_or_default("--port", default=8000))

TRAINING_CLIENTS = int(arg_or_default("--ml-training-clients", default=1))
TRAINING_FLOWS = int(arg_or_default("--ml-training-flows", default=1))

should_load_model = bool(arg_or_default("--load-model", default=False))

def load_model():
    if os.path.isfile(MODEL_PATH + MODEL_NAME + ".meta"):
        saver = tf.train.Saver()
        saver.restore(tf.get_default_session(), MODEL_PATH + MODEL_NAME)
    else:
        print("ERROR: Could not load model " + MODEL_PATH + MODEL_NAME)
        exit(-1)

model_params = model_param_set.ModelParameterSet(MODEL_NAME, MODEL_PATH)
env = pcc_env.PccEnv(model_params)

data_agg = data_aggregator.DataAggregator(TRAINING_FLOWS, TRAINING_CLIENTS, model_params,
env.observation_space.sample(), env.action_space.sample(), norm_rewards=True)

def policy_fn(name, ob_space, ac_space): #pylint: disable=W0613
    return LstmPolicy(
        name=name,
        ob_space=env.observation_space,
        ac_space=env.action_space,
        #hid_size=model_params.hidden_size,
        #num_hid_layers=model_params.hidden_layers,
        gaussian_fixed_var=True
    )

def train(data_agg, env, policy_fn, finished_queue):
    sess = U.single_threaded_session()
    sess.__enter__()
    trainer = trpo.TrpoTrainer(data_agg, env, policy_fn, 
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
    data_agg.give_dataset(dataset.data, block)
    return 0

server.register_introspection_functions()

server.register_function(give_dataset)

# Run the server's main loop
done = False
while not done:
    server.handle_request()
    done = not finished_queue.empty()

p.join()

server.server_close()

print("Training server closed")
exit(-1)
