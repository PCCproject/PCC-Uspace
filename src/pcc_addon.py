import tensorflow as tf
import time
import os
import sys
import json

n_fields = 5
inputs = tf.placeholder(dtype=tf.float32, shape=[1, n_fields])
output = tf.placeholder(dtype=tf.float32, shape=[None])

n_neurons_1 = 64
n_neurons_2 = 32
n_target = 1

sigma = 1
weight_initializer = tf.variance_scaling_initializer(mode="fan_avg", distribution="uniform", scale=sigma)
bias_initializer = tf.zeros_initializer()

W_hidden_1 = tf.Variable(weight_initializer([n_fields, n_neurons_1]))
bias_hidden_1 = tf.Variable(bias_initializer([n_neurons_1]))

W_hidden_2 = tf.Variable(weight_initializer([n_neurons_1, n_neurons_2]))
bias_hidden_2 = tf.Variable(bias_initializer([n_neurons_2]))

W_out = tf.Variable(weight_initializer([n_neurons_2, n_target]))
bias_out = tf.Variable(bias_initializer([n_target]))

hidden_1 = tf.nn.relu(tf.add(tf.matmul(inputs, W_hidden_1), bias_hidden_1))
hidden_2 = tf.nn.relu(tf.add(tf.matmul(hidden_1, W_hidden_2), bias_hidden_2))
out = tf.transpose(tf.add(tf.matmul(hidden_2, W_out), bias_out))

mse = tf.reduce_mean(tf.squared_difference(out, output))

# This line doesn't work because output is something we know (utility) but not something that the network ever produces.
opt = tf.train.AdamOptimizer().minimize(mse)

net = tf.Session()

if os.path.isfile("./pcc_model.nn.meta"):
    #print "Restoring session..."
    tf.train.Saver().restore(net, "./pcc_model.nn")
    #print "Loaded session:"
    out_weights = W_out.eval(session=net)
    #print out_weights
else:
    #print "Running network"
    net.run(tf.global_variables_initializer())

next_rate = 1.0

rate = -1.0
lat = 0.0
loss = 0.0
lat_infl = 0.0

rate_step = 5000000.0
latency_step = 1000.0
loss_step = 0.01
lat_infl_step = 0.01

context_table = {}

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
    print "USING PYTHON CONTEXT TABLE"

def give_reward(sending_rate, latency, loss_rate, latency_inflation, new_sending_rate, utility):
    #print "rate=" + str(sending_rate / 1000000.0) + ", lat=" + str(latency) + ", loss=" + str(loss_rate) + ", " + str(new_sending_rate / 1000000.0) + ", util=" + str(utility)
    global use_context_table
    if use_context_table:
        context_table_give_reward(rate, latency, loss, latency_inflation, new_sending_rate, utility);
    else:
        net.run(opt, feed_dict={inputs: [[sending_rate / 1000000.0, latency, loss_rate, latency_inflation, new_sending_rate / 1000000.0]], output: [utility]})

def give_sample(sending_rate, latency, loss_rate, latency_inflation, utility, auto_reward=True):
    global rate
    global lat
    global loss
    global lat_infl
    if (auto_reward == True) and (rate >= 0.0):
        give_reward(rate * 1000000.0, lat, loss, lat_infl, sending_rate, utility)
    rate = sending_rate / 1000000.0
    lat = latency
    loss = loss_rate
    lat_infl = latency_inflation

def predict_utility(new_sending_rate):
    global rate
    global lat
    global loss
    global lat_infl
    new_utility = net.run(out, feed_dict={inputs: [[rate, lat, loss, lat_infl, new_sending_rate]]})
    return new_utility[0][0]

def get_rate():
    global rate
    global lat
    global loss
    global lat_infl
    if use_context_table:
        return context_table_get_best_rate(rate, lat, loss, lat_infl)
    else:
        possible_rates = []
        possible_utilities = []
        for i in range(-3, 3):
            possible_rates.append(rate * (1.0 + 0.1 * i))
        for possible_rate in possible_rates:
            possible_utilities.append(predict_utility(possible_rate))
        best_utility = possible_utilities[0]
        best_rate = possible_rates[0]
        for i in range(1, len(possible_rates)):
            #print "Rate: " + str(possible_rates[i]) + ", Utility: " + str(possible_utilities[i])
            if (possible_utilities[i] > best_utility):
                best_utility = possible_utilities[i]
                best_rate = possible_rates[i]
        return best_rate * 1000000.0

def save_model():
    tf.train.Saver().save(net, "./pcc_model.nn")

# Sanity checking code below
def sanity_check():
    print "Giving reward..."
    start_time = time.time()
    for i in range(0, 10000):
        pass
        give_reward(99000000.0, 30000.0, 0.0, 0.0, 94000000.0, 94.0)
        give_reward(99000000.0, 30000.0, 0.0, 0.0, 104000000.0, 104.0)
    end_time = time.time()
    total_time = end_time - start_time
    print "Time elapsed = " + str(total_time) + ", time per reward = " + str(total_time / 20000.0)
    print "Giving sample..."
    give_sample(99000000.0, 30000.0, 0.0, 0.0, 99.0, False)
    print "Chosen rate is:" + str(get_rate())
    start_time = time.time()
    for i in range(0, 100):
        get_rate()
        pass
    end_time = time.time()
    total_time = end_time - start_time
    print "Time elapsed = " + str(total_time) + ", time per rate decision = " + str(total_time / 100.0)
    out_weights = W_out.eval(session=net)
    print out_weights
    save_model()

#sanity_check()
