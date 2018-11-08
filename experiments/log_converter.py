import time
import os
import json
import sys

def convert_log(filename):
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
    log = {"Log Version":"njay-1", "Events":events, "Experiment Parameters":{}}
    with open(new_filename, "w") as f:
        json.dump(log, f, indent=4)

convert_log(sys.argv[1])
