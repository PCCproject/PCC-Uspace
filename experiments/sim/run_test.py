import os
import sys

from config import pcc_config_njay as cfg

log = "pcc_log.txt"
log_dir = cfg.PCC_CONFIG["EXPR_DIR"] + "logs/"
os.system("mkdir " + log_dir)

pyhelper = "training_client"
pypath = cfg.PCC_CONFIG["PYTHON_ML_DIR"]
model_name = "cur_model"

cmd = cfg.PCC_CONFIG["SIM_DIR"] + "sim_test"
cmd += " --sim-dur=30"
cmd += " -log=" + log_dir + log
cmd += " -pyhelper=" + pyhelper
cmd += " -pypath=" + pypath
cmd += " --deterministic"
cmd += " --pcc-utility-calc=lin"
cmd += " --model-name=" + model_name
cmd += " --no-training"
cmd += " --log-utility-calc-lite"
cmd += " --pcc-rate-control=python"
cmd += " --no-reset"

for arg in sys.argv:
    if not arg.startswith("--env"):
        cmd += " " + arg

print(cmd)
os.system(cmd)

cmd = "sudo killall sim_test"
print(cmd)
os.system(cmd)

grapher = cfg.PCC_CONFIG["REPO_DIR"] + "vis/pcc_grapher.py"
graph_config = cfg.PCC_CONFIG["REPO_DIR"] + "vis/graphs/basic.json"

cmd = "python " + grapher + " " + log_dir + " " + graph_config

print(cmd)
os.system(cmd)
