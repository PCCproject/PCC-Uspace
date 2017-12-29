import sys
import math
import pcc_addon
import json

training_event_type = "Calculate Utility"

if (len(sys.argv) == 1):
    print "usage: pcc_grapher.py [options] \"<pcc_log_file> [<pcc_log_file_2>, ...]\"" 

log_files = sys.argv[1].split()
log_data = []
event_counts = []

def train_on_event(event):
    pcc_addon.give_sample(
        float(event["Target Rate"]),
        float(event["Avg RTT"]),
        float(event["Loss Rate"]),
        float(event["Latency Inflation"]),
        float(event["Utility"]),
        True)

def train_on_log_file(filename):
    data = json.load(open(filename))
    for event in data["events"]:
        event_name = event.keys()[0]
        if event_name == training_event_type:
            train_on_event(event[event_name])

for filename in log_files:
    train_on_log_file(filename)

pcc_addon.get_rate()
pcc_addon.save_model()
