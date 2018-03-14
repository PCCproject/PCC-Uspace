import pcc_gym_driver
import time
import json
import sys

#
# A static link state to give to the gym training algorithm. We will keep all of
# these parameters constant and define a reward function based on the utility to
# see if the sender can learn that particular reward.
#
lat = 30000.0
lat_infl = 0
prob_loss = 0

LINK_CAPACITY = 100e6

"""
for arg in sys.argv:
    arg_val = "NULL"
    if "=" in arg and "log=" not in arg and "model-name=" not in arg:
        arg_val = float(arg[arg.rfind("=") + 1:])

    if "--all-rate-scale=" in arg:
        LINK_CAPACITY *= arg_val

    if "--utility-scale=" in arg:
        UTILITY_SCALE = arg_val

    if "--latency-scale=" in arg:
        LATENCY_SCALE = arg_val

    if "--rate-scale=" in arg:
        RATE_SCALE = arg_val

    if "--loss-scale=" in arg:
        LOSS_SCALE = arg_val
"""

# The number of samples to train for
N_SAMPLES = 500000

# 
events = []
flat_args = ""
cfg = {}
log_name = "test_log.txt"
for arg in sys.argv:
    flat_args += " " + arg
    if "=" in arg:
        equals = arg.rfind("=")
        cfg[arg[:equals]] = arg[equals + 1:]
        if "-log=" in arg:
            log_name = arg[equals + 1:]
cfg["Param Args"] = flat_args
_time = 0

def loss_func(rate):
    if rate > LINK_CAPACITY:
        return (rate - LINK_CAPACITY) / rate
    return 0.0

#
# We are trying to train our policy neural network to choose the rate that
# is given the maximum reward.
#
def reward(rate):
    global events
    global _time
    loss = loss_func(rate)
    #util = rate
    #if rate > LINK_CAPACITY:
    #    util -= 2 * (rate - LINK_CAPACITY)
    util = rate * (1.0 - loss) * (1.0 - loss) * (1.0 - loss)
    events.append({"Calculate Utility":{"Utility":util, "Loss Rate":loss_func(rate), "Target Rate":rate, "Actual Rate":rate, "Time":_time}})
    _time += 1
    return util

# We will give N_SAMPLES samples to the training algorithm
for i in range(0, N_SAMPLES):
    
    # Record the starting time for a single rate-then-sample loop.
    start = time.time()

    #print("================")
    #print(" Time: " + str(i))
    #print("================")

    # Ask the training algorithm for the rate to send packets.
    rate = pcc_gym_driver.get_rate()
    #print(str(i) + " get_rate() = " + str(rate))

    # Calculate the reward the algorithm based on its chosen rate.
    util = reward(rate)

    stop = (i == (N_SAMPLES - 1))

    if (stop):
        print("SENDING STOP SIGNAL")
    # Give the algorithm information about the current link state and reward.
    pcc_gym_driver.give_sample(rate, lat, loss_func(rate), lat_infl, util, stop)
    #print(str(i) + " give_sample() (rate = " + str(rate))
    #pcc_gym_driver.give_sample(0.0, 0, 0, 0, rate, stop)

    # Record the endtime for a single training loop.
    end = time.time()
    
    # If desired, print the time of each training loop.
    #     (since we don't know the distribution, we just print every loop)
    #print("Decision time = " + str((end - start) * 1000.0))

json_obj = {"events":events, "Experiment Parameters":cfg}
with open("./logs/" + log_name, 'w') as outfile:
    json.dump(json_obj, outfile)
print("Main thread exiting")
exit(0)
