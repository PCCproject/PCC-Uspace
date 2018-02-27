import tensorflow as tf
import time
import os
import sys
import json
import numpy as np

input_history_len = 5
prediction_history_len = 2

n_fields = 5
n_inputs = n_fields * input_history_len + 1
inputs = tf.placeholder(dtype=tf.float32, shape=[1, n_inputs])
output = tf.placeholder(dtype=tf.float32, shape=[None])

n_neurons = [
    64,
    64,
    32,
    32,
    16,
    16,
    16
]

n_neurons_1 = 64
n_neurons_2 = 32
n_neurons_3 = 16
n_target = 1

sigma = 1
weight_initializer = tf.variance_scaling_initializer(mode="fan_avg", distribution="uniform", scale=sigma)
bias_initializer = tf.zeros_initializer()

w = []
bias = []
prev_size = n_inputs
for i in range(0, len(n_neurons)):
    w.append(tf.Variable(weight_initializer([prev_size, n_neurons[i]])))
    bias.append(tf.Variable(bias_initializer([n_neurons[i]])))
    prev_size = n_neurons[i]

W_out = tf.Variable(weight_initializer([n_neurons[-1], n_target]))
bias_out = tf.Variable(bias_initializer([n_target]))

hidden_layers = []
prev_layer = inputs
for i in range(0, len(n_neurons)):
    hidden_layers.append(tf.nn.relu(tf.add(tf.matmul(prev_layer, w[i]), bias[i])))
    prev_layer = hidden_layers[-1]

out = tf.transpose(tf.add(tf.matmul(hidden_layers[-1], W_out), bias_out))

mse = tf.reduce_mean(tf.squared_difference(out, output))

opt = tf.train.AdamOptimizer().minimize(mse)

net = tf.Session()

history_len = input_history_len + prediction_history_len
pypath = "./"
for arg in sys.argv:
    if "-pypath=" in arg:
        pypath = arg[8:]
if os.path.isfile(pypath + "pcc_model.nn.meta"):
    print "Restoring session..."
    tf.train.Saver().restore(net, pypath + "pcc_model.nn")
    #print "Loaded session:"
    out_weights = W_out.eval(session=net)
    #print out_weights
else:
    #print "Running network"
    net.run(tf.global_variables_initializer())

next_rate = 1.0
history = []

rate_step = 5000000.0
latency_step = 1000.0
loss_step = 0.01
lat_infl_step = 0.01

context_table = {}

always_learn_online = False
if "--online-learning" in sys.argv:
    always_learn_online = True

def round_rate(rate):
    global rate_step
    rate /= rate_step
    rate = float(int(rate))
    return rate * rate_step

def round_latency(latency):
    global latency_step
    latency /= latency_step
    latency = float(int(latency))
    return latency * latency_step

def round_loss(loss):
    global loss_step
    loss /= loss_step
    loss = float(int(loss))
    return loss * loss_step

def round_lat_infl(lat_infl):
    global lat_infl_step
    lat_infl /= lat_infl_step
    lat_infl = float(int(lat_infl))
    return lat_infl * lat_infl_step

def context_table_give_reward(sending_rate, latency, loss_rate, latency_inflation, new_sending_rate, utility):
    global context_table
    rate = str(round_rate(sending_rate))
    lat = str(round_latency(latency))
    loss = str(round_loss(loss_rate))
    lat_infl = str(round_lat_infl(latency_inflation))
    new_rate = str(round_rate(new_sending_rate))
    if not rate in context_table.keys():
        context_table[rate] = {}
    table = context_table[rate]

    if not lat in table.keys():
        table[lat] = {}
    table = table[lat]

    if not loss in table.keys():
        table[loss] = {}
    table = table[loss]

    if not lat_infl in table.keys():
        table[lat_infl] = {}
    table = table[lat_infl]

    if not new_rate in table.keys():
        table[new_rate] = {}
    utility_entry = table[new_rate]

    if not "samples" in utility_entry.keys():
        utility_entry["samples"] = 1
        utility_entry["value"] = utility
    else:
        utility_entry["samples"] += 1
        utility_entry["value"] = (utility_entry["value"] * (utility_entry["samples"] - 1) + utility) / utility_entry["samples"]

    if not "best" in table.keys():
        table["best"] = float(new_rate)
    else:
        if (table["best"] < float(new_rate)):
            table["best"] = float(new_rate)

def context_table_get_best_rate(rate, lat, loss, lat_infl):
    global context_table
    rate = str(round_rate(rate))
    lat = str(round_latency(lat))
    loss = str(round_loss(loss))
    lat_infl = str(round_lat_infl(lat_infl))
    if not rate in context_table.keys():
        context_table[rate] = {}
    table = context_table[rate]

    if not lat in table.keys():
        table[lat] = {}
    table = table[lat]

    if not loss in table.keys():
        table[loss] = {}
    table = table[loss]

    if not lat_infl in table.keys():
        table[lat_infl] = {}
    table = table[lat_infl]

    if "best" in table.keys():
        return table["best"]
    else:
        return 0.0

def load_context_table():
    global context_table
    with open('./pcc_context_table.json', 'r') as f:
        context_table = json.load(f)

def save_context_table():
    global context_table
    with open('./pcc_context_table.json', 'w') as f:
        json.dump(context_table, f)

use_context_table = False
if ("--python-context-table" in sys.argv):
    use_context_table = True
    load_context_table()
    print("USING PYTHON CONTEXT TABLE")

def train_on_dataset(dataset):
    net.run(opt, feed_dict={inputs:dataset["inputs"], output:dataset["output"]})

def give_reward():
    #print "rate=" + str(sending_rate / 1000000.0) + ", lat=" + str(latency) + ", loss=" + str(loss_rate) + ", " + str(new_sending_rate / 1000000.0) + ", util=" + str(utility)
    global use_context_table
    global prediction_history_len
    global history
    if use_context_table:
        #context_table_give_reward(rate, latency, loss, latency_inflation, new_sending_rate, utility);
        pass
    else:
        if (len(history) < history_len):
            return
        pred_inputs = flatten_history(prediction_history_len)
        #pred_inputs = pred_inputs[:-1]
        pred_inputs.append(history[-1 * prediction_history_len][0])
        utilities = []
        for i in range(1, prediction_history_len + 1):
            utilities.append(history[-1 * i][4])
        utility_avg = sum(utilities) / float(len(utilities))
        utility_var = np.var(utilities)
        utility_min = min(utilities)
        utility_max = max(utilities)
        utility_range = utility_max - utility_min
        #print "==================== "
        #print " Giving reward with inputs: "
        #print "==================== "
        #print pred_inputs
        #print "Reward: " + str(utility_avg)
        net.run(opt, feed_dict={inputs: [pred_inputs], output: [1000000000.0 * utility_avg]})

def give_sample(sending_rate, latency, loss_rate, latency_inflation, utility, auto_reward=False):
    update_history(sending_rate / 1000000000.0, latency / 1000000.0, loss_rate, latency_inflation, utility / 1000000000.0)
    if auto_reward or always_learn_online:
        give_reward()

def clear_history():
    global history
    history = []

def predict_utility(new_sending_rate):
    if len(history) < input_history_len:
        return -1.0
    pred_inputs = flatten_history(0)
    #pred_inputs = pred_inputs[:-1]
    pred_inputs.append(new_sending_rate)
    #print "==================== "
    #print " Predicting utility with inputs: "
    #print "==================== "
    #print pred_inputs
    new_utility = net.run(out, feed_dict={inputs: [pred_inputs]})
    return new_utility[0][0]

def update_history(rate, lat, loss, lat_infl, util):
    if (len(history) >= history_len):
        history.pop(0)
    history.append([rate, lat, loss, lat_infl, util])
    #print "==================== "
    #print " History updated to: "
    #print "==================== "
    #for h in history:
    #    print h

def flatten_history(offset):
    global history
    global input_history_len
    if offset == 0:
        return [item for sublist in history[-1 * input_history_len:] for item in sublist]
    return [item for sublist in history[-1 * input_history_len - offset: -1 * offset] for item in sublist]

def get_rate():
    global history
    rate = history[-1][0]
    if use_context_table:
        #return context_table_get_best_rate(rate, lat, loss, lat_infl)
        pass
    else:
        step = 0.05
        best_rate = rate
        best_utility = -1000000000.0
        for i in range(-9, 10):
            possible_rate = rate + i * step
            if (possible_rate > 0 and possible_rate < 0.4):
                expected_utility = predict_utility(possible_rate)
                if expected_utility > best_utility:
                    best_rate = possible_rate
                    best_utility = expected_utility

        best_prev_step_rate = best_rate
        step /= 10.0
        for i in range(-9, 10):
            possible_rate = best_prev_step_rate + i * step
            if (possible_rate > 0 and possible_rate < 0.4):
                expected_utility = predict_utility(possible_rate)
                if expected_utility > best_utility:
                    best_rate = possible_rate
                    best_utility = expected_utility
        
        if best_utility == -1:
            return rate * 1000000000.0
        return best_rate * 1000000000.0

def save_model():
    global pypath
    tf.train.Saver().save(net, pypath + "pcc_model.nn")
