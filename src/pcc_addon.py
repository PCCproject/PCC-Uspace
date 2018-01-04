import tensorflow as tf
import time
import os

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

#mse = tf.reduce_mean(# How do I tell it what rate it should have chosen? #)
mse = tf.reduce_mean(tf.squared_difference(out, output))

# This line doesn't work because output is something we know (utility) but not something that the network ever produces.
# opt = tf.train.AdamOptimizer().minimize(-1 * output)
opt = tf.train.AdamOptimizer().minimize(mse)

#print "Starting session"
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

def give_reward(sending_rate, latency, loss_rate, latency_inflation, new_sending_rate, utility):
    #print "rate=" + str(sending_rate / 1000000.0) + ", lat=" + str(latency) + ", loss=" + str(loss_rate) + ", " + str(new_sending_rate / 1000000.0) + ", util=" + str(utility)
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
