import time
import os
import json

def get_log_name(hist, depth, width):
    return "log_hist%d_depth%d_width%d" % (hist, depth, width) + ".txt"

def convert_log(hist, depth, width):
    filename = get_log_name(hist, depth, width)
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
    log = {"Log Version":"njay-1", "Events":events, "Experiment Parameters":{"Model":"H-%d, D-%d, W-%d" % (hist, depth, width)}}
    with open(new_filename, "w") as f:
        json.dump(log, f, indent=4)

model_params = [
    [3, 3, 32],
    [1, 3, 32],
    [2, 3, 32],
    [3, 0, 32],
    [3, 1, 32],
    [3, 2, 32],
    [3, 3, 1],
    [3, 3, 4],
    [3, 3, 16],
    [3, 3, 32],
    [3, 1, 4]
]

model_params = [[3, 3, 32]]

for p in model_params:
    convert_log(p[0], p[1], p[2])
