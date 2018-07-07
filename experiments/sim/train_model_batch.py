import time
import os

model_name = "cur_model"

def get_model_name(hist, depth, width, gamma, utility):
    return "model_h%d_d%d_w%d_g%f_u%s" % (hist, depth, width, gamma, utility)

def get_model_args(hist, depth, width, gamma, utility):
    return [
        "--history-len=%d" % hist,
        "--hid-layers=%d" % depth,
        "--hid-size=%d" % width,
        "--gamma=%f" % gamma,
        "--model-name=%s" % get_model_name(hist, depth, width, gamma, utility),
        "--pcc-utility-calc=%s" % utility
    ]

def get_log_name(hist, depth, width, gamma, utility):
    return "log_h%d_d%d_w%d_g%f_u%s" % (hist, depth, width, gamma, utility) + ".txt"

next_port = 8000
def run_training(hist, depth, width, gamma, utility, background=True):
    global next_port
    arg_str = ""
    for arg in get_model_args(hist, depth, width, gamma, utility):
        arg_str += arg + " "
    arg_str += " --ml-port=%d " % next_port
    next_port += 1
    cmd = "python ./sim/run_ml_training.py " + arg_str + ("1>%s" % get_log_name(hist, depth, width, gamma, utility))
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
    [2, 3, 32, 0.0, "linear"],
    [3, 3, 32, 0.0, "linear"],
    [5, 3, 32, 0.0, "linear"],
    [10, 3, 32, 0.0, "linear"],
    [100, 3, 32, 0.0, "linear"],
    [100, 5, 32, 0.0, "linear"],
    [100, 7, 32, 0.0, "linear"],
    [100, 9, 32, 0.0, "linear"],
    [10, 3, 32, 0.5, "linear"],
    [10, 3, 32, 0.75, "linear"],
    [10, 3, 32, 0.90, "linear"],
    [10, 3, 32, 0.95, "linear"]
]

for p in model_params:
    run_training(p[0], p[1], p[2], p[3], p[4], background=False)
    time.sleep(5)
