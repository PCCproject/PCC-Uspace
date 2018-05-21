import pcc_gym_driver as pcc_gym_driver
import time
import json
import sys
import math
import random

def calculate_utility(rate, lat, loss, lat_infl):
    alpha = 1.0 / 2500.0
    beta = 20.0
    util = rate / math.exp(alpha * lat + beta * loss) 
    #print("r = " + str(rate) + ", lat = " + str(lat) + ", loss = " + str(loss))
    return util

class SimulatedLink:
    def __init__(self, bw, dl, bf, plr):
        self.bw = bw
        self.dl = dl
        self.max_queue_dl = bf / bw
        self.cur_queue_dl = 0
        self.plr = plr

    def step(self, rate, dur):
        lat = self.dl
        loss = self.plr
        bw = self.bw
        cur_queue_dl = self.cur_queue_dl
        max_queue_dl = self.max_queue_dl
        r = rate * (1.0 - self.plr)
        end_queue_dl = cur_queue_dl + dur * (r - bw) / bw
        if (end_queue_dl < 0):
            t_empty = cur_queue_dl / (1 - r / bw)
            lat += (t_empty / dur) * (cur_queue_dl / 2)
            end_queue_dl = 0
        elif (end_queue_dl > max_queue_dl):
            t_fill = (max_queue_dl - cur_queue_dl) / (r / bw - 1)
            lat += (t_fill / dur) * ((cur_queue_dl + max_queue_dl) / 2) + max_queue_dl * ((dur - t_fill) / dur)
            end_queue_dl = max_queue_dl
            loss += (dur - t_fill) * (r - bw) / (rate * dur)
        else:
            lat += (end_queue_dl + cur_queue_dl) / 2

        lat_infl = (end_queue_dl - cur_queue_dl) / (cur_queue_dl + self.dl);
        self.cur_queue_dl = end_queue_dl;
        util = calculate_utility(rate, lat, loss, lat_infl);
        return (rate, lat, loss, lat_infl, util)
#
# A static link state to give to the gym training algorithm. We will keep all of
# these parameters constant and define a reward function based on the utility to
# see if the sender can learn that particular reward.
#
LINK_BW = 100e6
LINK_DL = 30000.0
LINK_BF = 750 * 8 * 1000
LINK_PLR = 0.0

LINK_BW_MIN = 100e6
LINK_BW_MAX = 100e6
LINK_CHANGE_INTERVAL = 500

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

link = SimulatedLink(LINK_BW, LINK_DL, LINK_BF, LINK_PLR)
prev_lat = 10000

# We will give N_SAMPLES samples to the training algorithm
for i in range(0, N_SAMPLES):
    
    # Record the starting time for a single rate-then-sample loop.
    start = time.time()

    #print("================")
    #print(" Time: " + str(i))
    #print("================")

    if (i % LINK_CHANGE_INTERVAL == 0):
        link.bw = random.uniform(LINK_BW_MIN, LINK_BW_MAX)

    # Ask the training algorithm for the rate to send packets.
    rate = pcc_gym_driver.get_rate()

    # Calculate the reward the algorithm based on its chosen rate.
    rate, lat, loss, lat_infl, util = link.step(rate, 1.5 * prev_lat)
    #print("r = " + str(rate) + ", u = " + str(util))
    events.append({"Actual Rate":rate, "Avg RTT":lat, "Loss Rate":loss, "Utility":util})

    stop = (i == (N_SAMPLES - 1))

    if (stop):
        print("SENDING STOP SIGNAL")
    # Give the algorithm information about the current link state and reward.
    pcc_gym_driver.give_sample(rate, lat, loss, lat_infl, util, stop)
    #print(str(i) + " give_sample() (rate = " + str(rate))
    link.step(rate, 1.0 * lat)
    prev_lat = lat

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
