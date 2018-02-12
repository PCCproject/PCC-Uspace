import os
import sys
import math
import matplotlib.pyplot as plt
import json
import numpy
import pcc_addon

point_size = 6.0
legend = ["Estimated Utility"]

def give_net_context(context):
    pcc_addon.give_sample(
        context["Target Rate"],
        context["Avg RTT"],
        context["Loss Rate"],
        context["Latency Inflation"],
        context["Vivace Latency Utility"],
        False)

def graph_net_estimates(title):
    x_values = []
    y_values = []
    for i in range(0, 200):
        x = 1000000.0 * i
        y = pcc_addon.predict_utility(x / 1000000000.0)
        x_values.append(x)
        y_values.append(y)
    fig, axes = plt.subplots(1, sharex=True)
    y_axis_name = "Expected Utility"
    x_axis_name = "Next Sending Rate"
    handles = []
    handle, = axes.plot(x_values, y_values)
    handles.append(handle)
    plt.legend(handles, legend)
    axes.set_ylabel(y_axis_name)
    axes.set_xlabel(x_axis_name)
    fig.suptitle(title)
    plt.show()

contexts = [
{"Target Rate":22400000.0,
"Avg RTT":30713.5,
"Loss Rate":0.0,
"Latency Inflation":-0.000776873,
"Vivace Latency Utility":15.4626,
"Title":"Estimated Utility By Rate (22% link bandwidth, low latency inflation)"},
{"Target Rate":179200000.0,
"Avg RTT":36209.9,
"Loss Rate":0.438701,
"Latency Inflation":0.0338401,
"Vivace Latency Utility":-60800000,
"Title":"Estimated Utility By Rate (179% link bandwidth, 3.3% latency inflation)"},
{"Target Rate":85100000.0,
"Avg RTT":32296.8,
"Loss Rate":0.0,
"Latency Inflation":-0.0525414,
"Vivace Latency Utility":52000000,
"Title":"Estimated Utility By Rate (85% link bandwidth, 5.3% latency deflation)"}
]

for context in contexts:
    give_net_context(context)
    graph_net_estimates(context["Title"])
