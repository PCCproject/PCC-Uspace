
import os
import sys
from config import pcc_config_njay as cfg

cmd = cfg.PCC_CONFIG["SIM_DIR"] + "/test"
cmd += " -pyhelper=training_client"
cmd += " -pypath=" + cfg.PCC_CONFIG["PYTHON_ML_DIR"]
cmd += " --pcc-utility-calc=linear"
cmd += " --model-path=" + cfg.PCC_CONFIG["ML_MODEL_PATH"]
cmd += " --model-name=cur_model"
cmd += " --pcc-rate-control=python"
cmd += " --sim-dur=1000000"

def run_endless_training(link):
   this_cmd = cmd
   this_cmd += " --test-bw=" + str(link["bw"])
   this_cmd += " --test-dl=" + str(link["dl"])
   this_cmd += " --test-buf=" + str(link["buf"])
   this_cmd += " --test-plr=" + str(link["plr"])
   log_path = cfg.PCC_CONFIG["ML_MODEL_PATH"] + "/logs/"
   this_cmd += " --ml-log=" + log_path + "train_log_%dbw_%fdl_%dbuf_%fplr" % (link["bw"], link["dl"], link["buf"], link["plr"])
   os.system("python " + cfg.PCC_CONFIG["PYTHON_EXPR_DIR"] + "/endless_trainer.py " + this_cmd + " &")

"""
bws = [1, 16, 64, 256]
dls = [0.001, 0.010, 0.100]
bufs = [1, 100, 1000]
plrs = [0.0, 0.01, 0.03]
"""

bws = [16, 64]
dls = [0.03, 0.06]
bufs = [500]
plrs = [0.00]

link_configs = []

for bw in bws:
    for dl in dls:
        for buf in bufs:
            for plr in plrs:
                link_configs.append({"bw":bw, "dl":dl, "buf":buf, "plr":plr})

for link_config in link_configs:
    run_endless_training(link_config)
