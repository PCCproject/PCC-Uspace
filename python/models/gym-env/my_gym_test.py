import pcc_gym_driver
import time

#
# A static link state to give to the gym training algorithm. We will keep all of
# these parameters constant and define a reward function based on the utility to
# see if the sender can learn that particular reward.
#
lat = 30000
lat_infl = 0
loss = 0

# The number of samples to train for
N_SAMPLES = 1000000

#
# We are trying to train our policy neural network to choose the rate that
# is given the maximum reward.
#
def reward(rate):
    util = rate
    if rate > 0.1:
        util -= 2 * (rate - 0.1)
    return util

# We will give N_SAMPLES samples to the training algorithm
for i in range(0, N_SAMPLES):
    
    # Record the starting time for a single rate-then-sample loop.
    start = time.time()

    # Ask the training algorithm for the rate to send packets.
    rate = pcc_gym_driver.get_rate()

    # Calculate the reward the algorithm based on its chosen rate.
    util = reward(rate)

    # Give the algorithm information about the current link state and reward.
    pcc_gym_driver.give_sample(rate, lat, loss, lat_infl, util)

    # Record the endtime for a single training loop.
    end = time.time()
    
    # If desired, print the time of each training loop.
    #     (since we don't know the distribution, we just print every loop)
    #print("Decision time = " + str((end - start) * 1000.0))
