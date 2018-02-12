import os
import sys
import math
import matplotlib.pyplot as plt
import json
import numpy

point_size = 6.0

title = "Neural Network Test Accuracy by Training Epoch (Vivace)"
data_files = [
    "train_log_7.txt",
#    "train_log_6.txt",
#    "train_log_5.txt",
#    "train_log_4.txt",
#    "train_log_3.txt",
#    "train_log_2.txt",
#    "train_log_1.txt"
]
legend = [
    "7 Layer NN",
#    "6 Layer NN",
#    "5 Layer NN",
#    "4 Layer NN",
#    "3 Layer NN",
#    "2 Layer NN",
#    "1 Layer NN"
]
x_axis_values = []
y_axis_values = []

for l in range(0, len(data_files)):
    lines = open(data_files[l]).readlines()
    this_log_y_axis_values = []
    this_log_x_axis_values = []
    i = 0
    for line in lines:
        this_log_y_axis_values.append(float(line))
        this_log_x_axis_values.append(i)
        i += 1
    y_axis_values.append(this_log_y_axis_values)
    x_axis_values.append(this_log_x_axis_values)

fig, axes = plt.subplots(1, sharex=True)
y_axis_name = "Test Error"
x_axis_name = "Training Epoch"
handles = []
for j in range(0, len(data_files)):
    handle, = axes.plot(x_axis_values[j], y_axis_values[j])
    handles.append(handle)
    plt.legend(handles, legend)
    axes.set_ylabel(y_axis_name)
    axes.set_xlabel(x_axis_name)

fig.suptitle(title)
lims = axes.get_ylim()
axes.set_ylim([0, 1.3 * lims[1]])
plt.show()
