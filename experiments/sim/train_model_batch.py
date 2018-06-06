import time
import os

model_name = "cur_model"

model_params = [
    "--history-len=3",
    "--hid-layers=3",
    "--hid-size=32"
]

def get_model_name(hist, depth, width):
    return "model_hist%d_depth%d_width%d" % (hist, depth, width)

def get_model_args(hist, depth, width):
    return [
        "--history-len=%d" % hist,
        "--hid-layers=%d" % depth,
        "--hid-size=%d" % width,
        "--model-name=%s" % get_model_name(hist, depth, width)
    ]

def get_log_name(hist, depth, width):
    return "log_hist%d_depth%d_width%d" % (hist, depth, width) + ".txt"

next_port = 8000
def run_training(hist, depth, width, background=True):
    global next_port
    arg_str = ""
    for arg in get_model_args(hist, depth, width):
        arg_str += arg + " "
    arg_str += " --ml-port=%d " % next_port
    next_port += 1
    cmd = "python ./sim/run_ml_training.py " + arg_str + (" 2>1 1>%s" % get_log_name(hist, depth, width))
    if background:
        cmd += " &"
    print(cmd)
    os.system(cmd)

model_params = [
    [3, 3, 32],
    [1, 3, 32],
    [2, 3, 32],
    [3, 0, 32],
    [3, 1, 32],
    [3, 3, 1],
    [3, 3, 4],
    [3, 3, 32]
]

for p in model_params:
    run_training(p[0], p[1], p[2])
    time.sleep(5)
