
import os
import sys
import subprocess
import time
from config import pcc_expr_config as cfg
import random
import signal

random.seed(int(time.time() * 1000000))

dur = 720000

n_replicas = 1
flows_per_link = 1

model_name = "cur_model"
for arg in sys.argv:
    if "--model-name=" in arg:
        model_name = arg[arg.find("=") + 1:]

cmd = [
    cfg.PCC_CONFIG["SIM_DIR"] + "/test",
    "-pyhelper=training_client",
    "-pypath=" + cfg.PCC_CONFIG["PYTHON_ML_DIR"],
    "--model-path=" + cfg.PCC_CONFIG["ML_MODEL_PATH"],
    "--pcc-rate-control=python",
    "--sim-dur=50000",
    "--pcc-utility-calc=linear"
]

cmd += sys.argv

def run_endless_training(link):
   this_cmd = list(cmd)
   this_cmd.append("--test-bw=%d" % link["bw"])
   this_cmd.append("--test-dl=%f" % link["dl"])
   this_cmd.append("--test-buf=%d" % link["buf"])
   this_cmd.append("--test-plr=%f" % link["plr"])
   this_cmd.append("--sim-latency-variation=%f" % link["lat_var"])
   this_cmd.append("--sim-n-copies=%d" % flows_per_link)
   if flows_per_link == 1:
       this_cmd.append("--ml-pause-during-trpo")
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
lat_vars = [0.02]

#bws = [16]
#dls = [0.016]

link_configs = []

for i in range(0, n_replicas):
    for bw in bws:
        for dl in dls:
            for buf in bufs:
                for plr in plrs:
                    for lat_var in lat_vars:
                        link_configs.append({"bw":bw, "dl":dl, "buf":buf, "plr":plr, "lat_var":lat_var})

server_cmd = [
    "python3",
    cfg.PCC_CONFIG["PYTHON_ML_DIR"] + "training_server.py",
    "--model-path=" + cfg.PCC_CONFIG["ML_MODEL_PATH"],
    "--ml-training-flows=%d" % (len(link_configs) * flows_per_link),
    "--ml-max-iters=1200",
    "--ts-per-batch=8192"
]

server_cmd += sys.argv

print(server_cmd)
server_proc = subprocess.Popen(server_cmd)

time.sleep(10)

client_procs = []
for link_config in link_configs:
    client_procs.append(run_endless_training(link_config))

def signal_handler(sig, frame):
    print("You pressed Ctrl+C! Training will now stop. The most recent version of your model will be saved.")
    for proc in client_procs:
        proc.kill()
    if server_proc.poll() is None:
        server_proc.kill()
    sys.exit(0)
signal.signal(signal.SIGINT, signal_handler)

time_slept = 0
sleep_increment = 1

done = False
while (time_slept < dur and not done):
    try:
        os.kill(server_proc.pid, 0)
    except ProcessLookupError as e:
        done = True
    time.sleep(sleep_increment)
    time_slept += sleep_increment

print("Server process finished polling")

for proc in client_procs:
    proc.kill()

if server_proc.poll() is None:
    server_proc.kill()

time.sleep(5)
print("Training finished")
