import sys
import math
import pcc_addon
import json

estimation_event_type = "Calculate Utility"


def estimate_event_utility(prev_event, event):
    pcc_addon.give_sample(
        float(prev_event["Target Rate"]),
        float(prev_event["Avg RTT"]),
        float(prev_event["Loss Rate"]),
        float(prev_event["Latency Inflation"]),
        float(prev_event["Vivace Latency Utility"]),
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
        
if (len(sys.argv) == 1):
    print "usage: pcc_log_esimator.py log.txt" 
log_file = sys.argv[1]
estimate_log_file(log_file)

