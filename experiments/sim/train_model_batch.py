import time
import os

model_name = "cur_model"

def get_model_name(hist, depth, width, gamma, utility, run_id):
    return "model_h%d_d%d_w%d_g%f_u%s_run%d" % (hist, depth, width, gamma, utility, run_id)

def get_model_args(hist, depth, width, gamma, utility, run_id):
    return [
        "--history-len=%d" % hist,
        "--hid-layers=%d" % depth,
        "--hid-size=%d" % width,
        "--gamma=%f" % gamma,
        "--model-name=%s" % get_model_name(hist, depth, width, gamma, utility, run_id),
        "--pcc-utility-calc=%s" % utility
    ]

def get_log_name(hist, depth, width, gamma, utility, run_id):
    return "log_h%d_d%d_w%d_g%f_u%s_%d" % (hist, depth, width, gamma, utility, run_id) + ".txt"

next_port = 8000
def run_training(hist, depth, width, gamma, utility, run_id, background=True):
    global next_port
    arg_str = ""
    for arg in get_model_args(hist, depth, width, gamma, utility, run_id):
        arg_str += arg + " "
    arg_str += " --ml-port=%d " % next_port
    next_port += 1
    cmd = "python ./sim/run_ml_training.py " + arg_str + ("1>%s" % get_log_name(hist, depth, width, gamma, utility, run_id))
    if background:
        cmd += " &"
    print(cmd)
    os.system(cmd)

model_params = [
    [10, 3, 32, "linear"],
    [10, 3, 32, "loss-only"],
    [10, 3, 32, "vivace"],
    [10, 3, 32, "copa"]
]

model_params = [
    [1, 3, 32, 0.0, "linear"],
    [5, 3, 32, 0.0, "linear"],
    [10, 3, 32, 0.0, "linear"],
    [100, 3, 32, 0.0, "linear"]
]

n_runs = 3
for p in model_params:
    for i in range(1, n_runs + 1):
    	run_training(p[0], p[1], p[2], p[3], p[4], i, background=(i < n_runs))
    	time.sleep(5)
