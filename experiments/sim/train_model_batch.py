import time
import os

model_name = "cur_model"

def get_model_name(hist, depth, width, utility):
    return "model_h%d_d%d_w%d_u%s" % (hist, depth, width, utility)

def get_model_args(hist, depth, width, utility):
    return [
        "--history-len=%d" % hist,
        "--hid-layers=%d" % depth,
        "--hid-size=%d" % width,
        "--model-name=%s" % get_model_name(hist, depth, width, utility),
        "--pcc-utility-calc=%s" % utility
    ]

def get_log_name(hist, depth, width, utility):
    return "log_h%d_d%d_w%d_u%s" % (hist, depth, width, utility) + ".txt"

next_port = 8000
def run_training(hist, depth, width, utility, background=True):
    global next_port
    arg_str = ""
    for arg in get_model_args(hist, depth, width, utility):
        arg_str += arg + " "
    arg_str += " --ml-port=%d " % next_port
    next_port += 1
    cmd = "python ./sim/run_ml_training.py " + arg_str + ("1>%s" % get_log_name(hist, depth, width, utility))
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

for p in model_params:
    run_training(p[0], p[1], p[2], p[3], background=False)
    time.sleep(5)
