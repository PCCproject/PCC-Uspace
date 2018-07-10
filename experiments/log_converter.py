import time
import os
import json

def get_log_name(hist, depth, width, gamma, utility):
    return "log_h%d_d%d_w%d_g%f_u%s" % (hist, depth, width, gamma, utility) + ".txt"

def convert_log(hist, depth, width, gamma, utility):
    filename = get_log_name(hist, depth, width, gamma, utility)
    lines = []
    with open(filename) as f:
        lines = f.readlines()

    events = []
    training_epoch = 0
    i = 0
    while i < len(lines):
        line = lines[i]
        if line[0:2] == "| ":
            event = {"Name":"Training Epoch", "Epoch":training_epoch}
            while i < len(lines) and line[0:2] == "| ":
                fields = line.split("|")
                event[fields[1].strip()] = fields[2].strip()
                i += 1
                line = lines[i]
            events.append(event) 	 
            training_epoch += 1
        else:
            i += 1
    new_filename = filename[:-3] + "log"
    log = {"Log Version":"njay-1", "Events":events, "Experiment Parameters":{"Model":"H-%d, D-%d, W-%d G-%f U-%s" % (hist, depth, width, gamma, utility)}}
    with open(new_filename, "w") as f:
        json.dump(log, f, indent=4)

model_params = [
    [1, 3, 32, 0.0, "linear"],
    [2, 3, 32, 0.0, "linear"],
    [3, 3, 32, 0.0, "linear"],
    [5, 3, 32, 0.0, "linear"],
    [10, 3, 32, 0.0, "linear"],
    [100, 3, 32, 0.0, "linear"],
    [100, 5, 32, 0.0, "linear"]
]

for p in model_params:
    convert_log(p[0], p[1], p[2], p[3], p[4])
