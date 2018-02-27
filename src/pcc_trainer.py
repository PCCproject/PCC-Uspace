import sys
import math
import pcc_addon
import json
import os

training_event_type = "Calculate Utility"
utility_func = "Inverted Exponent Utility"

if (len(sys.argv) == 1):
    print "usage: pcc_trainer.py dir_with_logs [iterations]" 

iterations = 1
if (len(sys.argv) > 2):
    iterations = int(sys.argv[2])

log_files = os.listdir(sys.argv[1])

estimation_event_type = "Calculate Utility"

"""
def estimate_event_utility(prev_event, event):
    pcc_addon.give_sample(
        float(prev_event["Target Rate"]),
        float(prev_event["Avg RTT"]),
        float(prev_event["Loss Rate"]),
        float(prev_event["Latency Inflation"]),
        float(prev_event[utility_func]),
        False)

    return pcc_addon.predict_utility(float(event["Target Rate"]) / 1000000000.0)

def estimate_log_file(filename):
    data = json.load(open(filename))
    prev_event = None
    for event in data["events"]:
        event_name = event.keys()[0]
        ev = event[event_name]
        if event_name == estimation_event_type:
            if prev_event is None:
                ev["Estimate"] = "0.0"
            else:
                ev["Estimate"] = str(estimate_event_utility(prev_event, ev))
            prev_event = ev
    with open("est_" + filename, "w") as outfile:
        json.dump(data, outfile, indent=4)
"""
def give_event_sample(event):
    pcc_addon.give_sample(
        float(event["Target Rate"]),
        float(event["Avg RTT"]),
        float(event["Loss Rate"]),
        float(event["Latency Inflation"]),
        float(event[utility_func]),
        False)

def estimate_log_file(filename):
    pcc_addon.clear_history()
    data = json.load(open(filename))
    for event in data["events"]:
        event_name = event.keys()[0]
        ev = event[event_name]
        if event_name == estimation_event_type:
            ev["Estimate"] = str(pcc_addon.predict_utility(float(ev["Target Rate"]) / 1000000000.0))
            give_event_sample(ev)
    with open("est_" + filename, "w") as outfile:
        json.dump(data, outfile, indent=4)
    pcc_addon.clear_history()

def train_on_event(event):
    pcc_addon.give_sample(
        float(event["Target Rate"]),
        float(event["Avg RTT"]),
        float(event["Loss Rate"]),
        float(event["Latency Inflation"]),
        float(event[utility_func]),
        True)

def prepare_dataset(filename):
    data = json.load(open(filename))
    inputs = []
    output = []
    prev_event = None
    for event in data["events"]:
        event_name = event.keys()[0]
        ev = event[event_name]
        if event_name == training_event_type:
            if prev_event is not None:
                old_rate = float(prev_event["Target Rate"]) / 1000000000.0
                avg_rtt = float(prev_event["Avg RTT"]) / 1000000.0
                loss_rate = float(prev_event["Loss Rate"])
                lat_infl = float(prev_event["Latency Inflation"])
                new_rate = float(ev["Target Rate"]) / 1000000000.0
                inputs.append([old_rate, avg_rtt, loss_rate, lat_infl, new_rate])
                output.append(float(ev[utility_func]))
            prev_event = ev
     
    dataset = {"inputs":inputs, "output":output}
    #return dataset
    return data

def train_on_dataset(dataset):
    #pcc_addon.train_on_dataset(dataset)
    #"""
    for event in dataset["events"]:
        event_name = event.keys()[0]
        if event_name == training_event_type:
            train_on_event(event[event_name])
    #"""

"""
def compute_event_error(event):
    val = float(event[utility_func])
    est = float(event["Estimate"])
    if est == 0.0:
        return 0.0
    return 0.5 * math.sqrt((val - est) * (val - est))


def compute_log_estimate_error(estimation):
    error = 0.0
    event_count = 0
    for event in estimation["events"]:
        event_name = event.keys()[0]
        if event_name == training_event_type:
            error += compute_event_error(event[event_name])
            event_count += 1
    return error / event_count
"""
def compute_event_error(events, i):
    est = float(events[i]["Estimate"])
    if est == -1.0 or len(events) < i + pcc_addon.prediction_history_len:
        return -1.0
    utility_sum = 0
    utility_count = 0
    for j in range(0, pcc_addon.prediction_history_len):
        utility_sum += float(events[i + j][utility_func])
        utility_count += 1
    val = utility_sum / float(utility_count)

    return 0.5 * math.sqrt((val - est) * (val - est))

def compute_log_estimate_error(estimation):
    error = 0.0
    event_count = 0
    events = []
    for event in estimation["events"]:
        event_name = event.keys()[0]
        if event_name == training_event_type:
            events.append(event[event_name])

    for i in range(0, len(events)):
        this_error = compute_event_error(events, i)
        if this_error >= 0:
            error += this_error
            event_count += 1
    return error / event_count

def test_on_data_set(filename):
    estimate_log_file(filename)
    estimation = json.load(open("est_" + filename))
    error = compute_log_estimate_error(estimation)
    print str(error)
    #print "Error: " + str(error)

datasets = []
for log_file in log_files:
    datasets.append(prepare_dataset(sys.argv[1] + "/" + log_file))

for i in range(0, iterations):
    #print "\t--- Iteration " + str(i) + " ---"
    if i % 10 == 0:
        if len(sys.argv) > 3:
            test_on_data_set(sys.argv[3])
    for dataset in datasets:
        train_on_dataset(dataset)

pcc_addon.save_model()
