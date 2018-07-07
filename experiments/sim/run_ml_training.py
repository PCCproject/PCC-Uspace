
import os
import sys
import subprocess
import time
from config import pcc_config_njay as cfg
import random

random.seed(int(time.time() * 1000000))

dur = 7200

n_replicas = 1
flows_per_link = 2

model_name = "cur_model"
for arg in sys.argv:
    if "--model-name=" in arg:
        model_name = arg[arg.find("=") + 1:]

cmd = [
    cfg.PCC_CONFIG["SIM_DIR"] + "/test",
    "-pyhelper=training_client",
    "-pypath=" + cfg.PCC_CONFIG["PYTHON_ML_DIR"],
    "--pcc-utility-calc=linear",
    "--model-path=" + cfg.PCC_CONFIG["ML_MODEL_PATH"],
    "--pcc-rate-control=python",
    "--sim-dur=50000"
]

cmd += sys.argv

def run_endless_training(link):
   this_cmd = list(cmd)
   this_cmd.append("--test-bw=%d" % link["bw"])
   this_cmd.append("--test-dl=%f" % link["dl"])
   this_cmd.append("--test-buf=%d" % link["buf"])
   this_cmd.append("--test-plr=%f" % link["plr"])
   this_cmd.append("--sim-n-copies=%d" % flows_per_link)
   this_cmd.append("--nonce=%d" % random.randint(1, 2e9))
   log_path = cfg.PCC_CONFIG["ML_MODEL_PATH"] + "/logs/"
   os.system("mkdir " + log_path)
   this_cmd.append(" --ml-log=" + log_path + "train_log_" + model_name + "_%dbw_%fdl_%dbuf_%fplr" % (link["bw"], link["dl"], link["buf"], link["plr"]))
   full_cmd = ["python", cfg.PCC_CONFIG["EXPR_DIR"] + "sim/endless_trainer.py"] + this_cmd
   print(full_cmd)
   return subprocess.Popen(full_cmd)

"""
bws = [1, 16, 64, 256]
dls = [0.001, 0.010, 0.100]
bufs = [1, 100, 1000]
plrs = [0.0, 0.01, 0.03]
"""

bws = [16, 64]
dls = [0.016, 0.064]
bufs = [500]
plrs = [0.00]

link_configs = []

for i in range(0, n_replicas):
    for bw in bws:
        for dl in dls:
            for buf in bufs:
                for plr in plrs:
                    link_configs.append({"bw":bw, "dl":dl, "buf":buf, "plr":plr})

server_cmd = [
    "python3",
    cfg.PCC_CONFIG["PYTHON_ML_DIR"] + "training_server.py",
    "--model-path=" + cfg.PCC_CONFIG["ML_MODEL_PATH"],
    "--ml-cp-freq=5",
    "--ml-cp-dir=/home/njay2/PCC/deep-learning/python/models/gym-rpc/models/checkpoints/",
    "--ml-training-clients=%d" % len(link_configs),
    "--ml-training-flows=%d" % (len(link_configs) * flows_per_link),
    "--ml-max-iters=3000"
]

server_cmd += sys.argv

print(server_cmd)
server_proc = subprocess.Popen(server_cmd)

time.sleep(10)

client_procs = []
for link_config in link_configs:
    client_procs.append(run_endless_training(link_config))

time_slept = 0
sleep_increment = 1

while (time_slept < dur and server_proc.poll() is None):
    time.sleep(sleep_increment)
    time_slept += sleep_increment

print("Server process finished polling")

for proc in client_procs:
    proc.kill()

if server_proc.poll() is None:
    server_proc.kill()

time.sleep(5)
print("Training finished")
