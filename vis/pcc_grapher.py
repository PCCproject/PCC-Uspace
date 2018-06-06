import os
import sys
import math
file_output = False
output_filename = None
for arg in sys.argv:
    if "--output=" in arg:
        output_filename = arg[arg.rfind("=") + 1:]
        file_output = True

if file_output:
    import matplotlib as mpl
    mpl.use('Agg')
import matplotlib.pyplot as plt
import json
import numpy
from scipy import interpolate
from scipy.signal import savgol_filter
from operator import itemgetter

def smooth(array, period):
    ret = numpy.cumsum(array, dtype=float)
    ret[period:] = ret[period:] - ret[:-period]
    ret[period - 1:] = ret[period - 1:] / period
    ret[:period] = array[:period]
    return ret[period:]

from analysis.pcc_experiment_log import *
from analysis.pcc_filter import *
from analysis.pcc_log_summary import *

def merge_log_group(log_group):
    merged_log = PccExperimentLog("NULL")
    merged_log.filename = "many"
    merged_log.dict = {}
    merged_log.event_dict = {}
    merged_log.event_types = ["Calculate Utility"]
    all_events = []
    for log in log_group:
        el = log.get_event_list("Calculate Utility")
        for e in el:
            e["Time"] = float(e["Time"])
        all_events += el
    #all_events.sort(key=itemgetter('Time'))
    #all_events = sorted(all_events, key=lambda k: k["Time"])
    all_events = sorted(all_events, key=itemgetter('Time'))
    merged_log.event_dict["Calculate Utility"] = all_events
    return merged_log

point_size = 30.0

if (len(sys.argv) == 1):
    print "usage: pcc_grapher.py <log_file_directory> <graph_json_file>"

graph_config = json.load(open(sys.argv[2]))
legend_param = graph_config["legend param"]
title = graph_config["title"] + " (by " + legend_param + ")"
event_type = graph_config["event"]
y_axis_names = graph_config["y-axis"]

should_smooth = False
smooth_window_size = None
if "smooth" in graph_config.keys():
    should_smooth = True
    smooth_window_size = int(graph_config["smooth"])

compression = 0
if "compression" in graph_config.keys():
    compression = graph_config["compression"]

log_files = os.listdir(sys.argv[1])
experiment_logs = []
event_counts = []
for filename in log_files:
    log = PccExperimentLog(sys.argv[1] + "/" + filename)
    if len(log.dict.keys()) > 0:
        log.apply_timeshift()
        experiment_logs.append(log)

if "log filters" in graph_config.keys():
    pcc_filter = PccLogFilter(graph_config["log filters"])
    print "Before filtering, have " + str(len(experiment_logs)) + " logs"
    experiment_logs = pcc_filter.apply_filter(experiment_logs)
    print "After filtering, have " + str(len(experiment_logs)) + " logs"

event_filters = None
if "event filters" in graph_config.keys():
    for event_filter_obj in graph_config["event filters"]:
        event_filter = PccEventFilter(event_filter_obj)
        for log in experiment_logs:
            event_filter.apply_filter(log)

for log in experiment_logs:
    if event_type not in log.get_event_types():
        experiment_logs.remove(log)

legend = []
for log in experiment_logs:
    legend.append(str(log.get_param(legend_param)))

model = None
if "model" in graph_config.keys():
    model = graph_config["model"]
    import pcc_addon

pypath = None
if "pypath" in graph_config.keys():
    pypath = graph_config["pypath"]

scatter = False
if "points" in graph_config.keys():
    if graph_config["points"] == "scatter":
        scatter = True

if graph_config["type"] == "summary":
    legend = []
    log_groups = []
    group_filters = []
    log_group_names = []
    if "groups" in graph_config.keys():
        group_filter_objs = graph_config["groups"]
        for group_filter_obj in group_filter_objs:
            print "Making group filter from " + str(group_filter_obj)
            #exit(1)
            group_filters.append(PccLogFilter(group_filter_obj["log filter"]))
    if len(group_filters) > 0:
        for group_filter in group_filters:
            log_groups.append(group_filter.apply_filter(experiment_logs))
            legend.append(group_filter.get_legend_label())
    else:
        log_groups = [experiment_logs]
    
    print "Found " + str(len(log_groups)) + " log groups!"
    for log_group in log_groups:
        print "Log group has " + str(len(log_group)) + " logs."
    log_group_x_values = []
    log_group_y_values = []
    for log_group in log_groups:
        experiment_logs = log_group
        x_axis_name = ""
        x_axis_values = []
        if "param" in graph_config["x-axis"].keys():
            x_axis_name = graph_config["x-axis"]["param"]
            for log in experiment_logs:
                print("value is " + log.get_param(x_axis_name))
                x_axis_values.append(float(log.get_param(x_axis_name)))

        if "stat" in graph_config["x-axis"].keys():
            x_axis_obj = graph_config["x-axis"]
            for log in experiment_logs:
                log_summary = PccLogSummary(log)
                event_summary = log_summary.get_event_summary(graph_config["event"])
                x_axis_values.append(event_summary.get_summary_stat(
                    x_axis_obj["value"],
                    x_axis_obj["stat"]))
        
        y_axis_values = []
        y_axis_objs = graph_config["y-axis"]
        
        for i in range(0, len(y_axis_objs)):
            y_axis_values.append([])
            y_axis_obj = y_axis_objs[i]
            for log in experiment_logs:
                log_summary = PccLogSummary(log)
                print("Getting event summary " + str(i) + "/" + str(len(y_axis_objs)))
                event_summary = log_summary.get_event_summary(graph_config["event"])
                y_axis_values[i].append(event_summary.get_summary_stat(
                    y_axis_obj["value"],
                    y_axis_obj["stat"]))
        log_group_x_values.append(x_axis_values)
        log_group_y_values.append(y_axis_values)

    fig, axes = plt.subplots(len(y_axis_values), sharex=True)
    fig.set_size_inches(1, 1)
    for i in range(0, len(y_axis_values)):
        handles = []
        y_axis_obj = y_axis_objs[i]
        y_axis_name = y_axis_obj["stat"] + " " + y_axis_obj["value"]
        for j in range(0, len(log_groups)):
            print("Graphing log group " + str(j))
            x_axis_values = log_group_x_values[j]
            y_axis_values = log_group_y_values[j]
            if len(y_axis_values) > 1:
                if "Time" in x_axis_name:
                    handle, = axes[i].plot(x_axis_values, y_axis_values[i])
                else: 
                    handle = axes[i].scatter(x_axis_values, y_axis_values[i], s=point_size)
                    if "annotate" in graph_config.keys():
                        for k in range(0, len(log_groups[j])):
                            log = log_groups[j][k]
                            annotation = log.filename[log.filename.rfind("_") + 1:-4]
                            axes[i].annotate(annotation, (x_axis_values[k], y_axis_values[i][k]))
                handles.append(handle)
                axes[i].set_ylabel(y_axis_name)
            else:
                if "Time" in x_axis_name:
                    handle, = axes.plot(x_axis_values, y_axis_values[i])
                else:
                    handle = axes.scatter(x_axis_values, y_axis_values[i], s=point_size)
                handles.append(handle)
                axes.set_ylabel(y_axis_name)
        plt.legend(handles, legend)
    
    if len(y_axis_names) > 1:
        axes[-1].set_xlabel(x_axis_name)
    else:
        axes.set_xlabel(x_axis_name)
    
    fig.suptitle(title)
    if file_output:
        plt.savefig(output_filename, figsize=(20, 6), dpi=300)
    else:
        plt.show()

if graph_config["type"] == "event":
    group_filters = []
    if "groups" in graph_config.keys():
        group_filter_objs = graph_config["groups"]
        for group_filter_obj in group_filter_objs:
            print "Making group filter from " + str(group_filter_obj)
            #exit(1)
            group_filters.append(PccLogFilter(group_filter_obj["log filter"]))
    legend = []
    if len(group_filters) > 0:
        new_logs = []
        for group_filter in group_filters:
            new_logs.append(merge_log_group(group_filter.apply_filter(experiment_logs)))
            legend.append(group_filter.get_legend_label())
        experiment_logs = new_logs     
    else:
        for log in experiment_logs:
            legend.append(str(log.get_param(legend_param)))
    x_axis_name = graph_config["x-axis"]

    avg_thpts = []
    loss_rates = []
    queue_lengths = []
    latencies = []
    x_axis_values = []
    y_axis_values = []
    model_event_time = 0.0
    for l in range(0, len(experiment_logs)):
        experiment_log = experiment_logs[l]
        this_log_x_axis_values = []
        this_log_y_axis_values = {}
        for y_axis_name in y_axis_names:
            this_log_y_axis_values[y_axis_name] = []
        for event in experiment_log.get_event_list(event_type):
            this_event = event
            this_log_x_axis_values.append(float(this_event[x_axis_name]))
            for k in this_log_y_axis_values.keys():
                this_log_y_axis_values[k].append(float(this_event[k]))
        if (should_smooth):
            for k in this_log_y_axis_values.keys():
                x = this_log_x_axis_values
                y = smooth(this_log_y_axis_values[k], smooth_window_size)
                this_log_y_axis_values[k] = y
                #y = this_log_y_axis_values[k]
                #if len(y) < 1:
                #    continue
                #f = interpolate.interp1d(x, y, kind="linear")
                #window_size = smooth_window_size
                #order = 3
                #this_log_y_axis_values[k] = savgol_filter(y, window_size, order)
            this_log_x_axis_values = this_log_x_axis_values[smooth_window_size:]
        x_axis_values.append(this_log_x_axis_values)
        y_axis_values.append(this_log_y_axis_values) 
        

    add_model_plot = (model is not None)
    n_plots = len(y_axis_values[0].keys())
    if add_model_plot:
        n_plots += 1
    fig, axes = plt.subplots(n_plots)#, sharex=True)
    fig.set_size_inches(10, 8)
    for i in range(0, len(y_axis_names)):
        y_axis_name = y_axis_names[i]
        handles = []
        for j in range(0, len(experiment_logs)):
            #if y_axis_name == "Inverted Exponent Utility":
            #    y_axis_values[j][y_axis_name] = numpy.log10(y_axis_values[j][y_axis_name])
            if len(y_axis_names) > 1:
                if scatter:
                    handle = axes[i].scatter(x_axis_values[j], y_axis_values[j][y_axis_name], s=point_size)
                else:
                    if "point event" in graph_config.keys():
                        graph_y_min = min(y_axis_values[j][y_axis_name])
                        point_event = graph_config["point event"]
                        point_event_x_values = []
                        point_event_y_values = []
                        k = 0
                        for event in experiment_logs[j].get_event_list(point_event):
                            print "Event " + str(k) + "/" + str(len(experiment_logs[j].get_event_list(point_event)))
                            k += 1
                            point_event_x_values.append(float(event["Time"]))
                            point_event_y_values.append(graph_y_min)
                        axes[i].scatter(point_event_x_values, point_event_y_values)
                    handle, = axes[i].plot(x_axis_values[j], y_axis_values[j][y_axis_name])
                handles.append(handle)
                plt.legend(handles, legend)
                axes[i].set_ylabel(y_axis_name)
                if add_model_plot:
                    axes[i].axvline(model_event_time, color='r')
            else:
                if scatter:
                    handle = axes.scatter(x_axis_values[j], y_axis_values[j][y_axis_name], s=point_size)
                else:
                    if "point event" in graph_config.keys():
                        graph_y_min = min(y_axis_values[j][y_axis_name])
                        point_event = graph_config["point event"]
                        point_event_x_values = []
                        point_event_y_values = []
                        k = 0
                        for event in experiment_logs[j].get_event_list(point_event):
                            print "Event " + str(k) + "/" + str(len(experiment_logs[j].get_event_list(point_event)))
                            k += 1
                            point_event_x_values.append(float(event["Time"]))
                            point_event_y_values.append(graph_y_min)
                        axes[i].scatter(point_event_x_values, point_event_y_values)
                    handle, = axes.plot(x_axis_values[j], y_axis_values[j][y_axis_name])
                handles.append(handle)
                plt.legend(handles, legend)
                axes.set_ylabel(y_axis_name)
                axes.axhline(4e7 - 4.5e7, linestyle="dashed", color="b")
    
    if add_model_plot:
        graph_net_estimates(axes[-1]) 
        if len(y_axis_names) > 1:
            axes[-2].set_xlabel(x_axis_name)
        pass
    else:
        if len(y_axis_names) > 1:
            axes[-1].set_xlabel(x_axis_name)
        else:
            axes.set_xlabel(x_axis_name)
    fig.suptitle(title)
    if file_output:
        plt.savefig(output_filename, figsize=(20, 6), dpi=300)
    else:
        plt.show()
