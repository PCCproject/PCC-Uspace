import os
import sys

from config import pcc_config_njay as cfg

log = "pcc_log.txt"
log_dir = cfg.PCC_CONFIG["EXPR_DIR"] + "logs/"
os.system("mkdir " + log_dir)

pyhelper = "training_client"
pypath = cfg.PCC_CONFIG["PYTHON_ML_DIR"]
model_name = "cur_model"
model_name = "model_h10_d3_w32_ulinear"
model_name = "model_h10_d3_w32_uloss-only"
model_name = "model_h10_d3_w32_ucopa"
model_name = "model_h10_d3_w32_uvivace"
#model_name = "saved_models/m1/cur_model"

cmd = cfg.PCC_CONFIG["SIM_DIR"] + "sim_test"
cmd += " --sim-dur=300"
cmd += " -log=" + log_dir + log
cmd += " -pyhelper=" + pyhelper
cmd += " -pypath=" + pypath
cmd += " --deterministic"
cmd += " --pcc-utility-calc=vivace"
cmd += " --model-path=" + cfg.PCC_CONFIG["ML_MODEL_PATH"]
cmd += " --model-name=" + model_name
cmd += " --no-training"
cmd += " --log-utility-calc-lite"
cmd += " --pcc-rate-control=python"
cmd += " --no-reset"

for arg in sys.argv[1:]:
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
cmd += " --output=graph.png"

print(cmd)
os.system(cmd)
