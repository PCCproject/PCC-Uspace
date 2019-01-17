import gym
from gym import spaces
from gym.utils import seeding
from gym.envs.registration import register
import numpy as np
import heapq
import time
import random
import json

MAX_RATE = 1000
MIN_RATE = 40

DELTA_SCALE = 0.025# * 1e-12
REWARD_SCALE = 0.01

EVENT_TYPE_SEND = 'S'
EVENT_TYPE_ACK = 'A'

BYTES_PER_PACKET = 1500

RATE_OBS_SCALE = 0.001
LAT_OBS_SCALE = 0.1

# The monitor interval class used to pass data from the PCC subsystem to
# the machine learning module.
#
class SenderMonitorInterval():
    def __init__(self, rate=0.0, recv_rate=0.0, latency=0.0, loss=0.0, lat_infl=0.0, lat_ratio=1.0, send_ratio=1.0):
        self.rate = rate
        self.recv_rate = recv_rate
        self.latency = latency
        self.loss = loss
        self.lat_infl = lat_infl
        self.lat_ratio = lat_ratio
        self.send_ratio = send_ratio

    # Convert the observation parts of the monitor interval into a numpy array
    def as_array(self, scale_free_only):
        if scale_free_only:
            return np.array([self.lat_infl, self.lat_ratio, self.send_ratio])
        else:
            return np.array([self.rate, self.recv_rate, self.latency, self.loss, self.lat_infl,
                self.lat_ratio, self.send_ratio])

class SenderHistory():
    def __init__(self, length, scale_free_only=True):
        self.scale_free_only = scale_free_only
        self.values = []
        for i in range(0, length):
            self.values.append(SenderMonitorInterval())

    def step(self, new_mi):
        self.values.pop(0)
        self.values.append(new_mi)

    def as_array(self):
        arrays = []
        for mi in self.values:
            arrays.append(mi.as_array(self.scale_free_only))
        arrays = np.array(arrays).flatten()
        return arrays

class Link():

    def __init__(self, bandwidth, delay, queue_size, loss_rate):
        self.bw = float(bandwidth)
        self.dl = delay
        self.lr = loss_rate
        self.queue_delay = 0.0
        self.queue_delay_update_time = 0.0
        self.max_queue_delay = queue_size / self.bw

    def get_cur_queue_delay(self, event_time):
        return max(0.0, self.queue_delay - (event_time - self.queue_delay_update_time))

    def get_cur_latency(self, event_time):
        return self.dl + self.get_cur_queue_delay(event_time)

    def packet_enters_link(self, event_time):
        if (random.random() < self.lr):
            return False
        self.queue_delay = self.get_cur_queue_delay(event_time)
        self.queue_delay_update_time = event_time
        extra_delay = 1.0 / self.bw
        #print("Extra delay: %f, Current delay: %f, Max delay: %f" % (extra_delay, self.queue_delay, self.max_queue_delay))
        if extra_delay + self.queue_delay > self.max_queue_delay:
            #print("\tDrop!")
            return False
        self.queue_delay += extra_delay
        #print("\tNew delay = %f" % self.queue_delay)
        return True

    def print_debug(self):
        print("Link:")
        print("Bandwidth: %f" % self.bw)
        print("Delay: %f" % self.dl)
        print("Queue Delay: %f" % self.queue_delay)
        print("Max Queue Delay: %f" % self.max_queue_delay)
        print("One Packet Queue Delay: %f" % (1.0 / self.bw))

    def reset(self):
        self.queue_delay = 0.0
        self.queue_delay_update_time = 0.0

class Network():
    
    def __init__(self, senders, links):
        self.q = []
        self.cur_time = 0.0
        self.senders = senders
        self.links = links
        self.queue_initial_packets()

    def queue_initial_packets(self):
        for sender in self.senders:
            sender.register_network(self)
            sender.reset_obs()
            heapq.heappush(self.q, (1.0 / sender.rate, sender, EVENT_TYPE_SEND, 0, 0.0, False)) 

    def reset(self):
        self.cur_time = 0.0
        self.q = []
        [link.reset() for link in self.links]
        [sender.reset() for sender in self.senders]
        self.queue_initial_packets()

    def get_cur_time(self):
        return self.cur_time

    def run_for_dur(self, dur):
        end_time = self.cur_time + dur
        for sender in self.senders:
            sender.reset_obs()

        while self.cur_time < end_time:
            event_time, sender, event_type, next_hop, cur_latency, dropped = heapq.heappop(self.q)
            #print("Got event %s, to link %d, latency %f at time %f" % (event_type, next_hop, cur_latency, event_time))
            self.cur_time = event_time
            new_event_time = event_time
            new_event_type = event_type
            new_next_hop = next_hop
            new_latency = cur_latency
            new_dropped = dropped
            push_new_event = False

            if event_type == EVENT_TYPE_ACK:
                if next_hop == len(sender.path):
                    if dropped:
                        sender.on_packet_lost()
                        #print("Packet lost at time %f" % self.cur_time)
                    else:
                        sender.on_packet_acked(cur_latency)
                        #print("Packet acked at time %f" % self.cur_time)
                else:
                    new_next_hop = next_hop + 1
                    link_latency = sender.path[next_hop].get_cur_latency(self.cur_time)
                    new_latency += link_latency
                    new_event_time += link_latency
                    push_new_event = True
            if event_type == EVENT_TYPE_SEND:
                if next_hop == 0:
                    #print("Packet sent at time %f" % self.cur_time)
                    sender.on_packet_sent()
                    heapq.heappush(self.q, (self.cur_time + (1.0 / sender.rate), sender, EVENT_TYPE_SEND, 0, 0.0, False))

                if next_hop == sender.dest:
                    new_event_type = EVENT_TYPE_ACK
                new_next_hop = next_hop + 1
                
                link_latency = sender.path[next_hop].get_cur_latency(self.cur_time)
                new_latency += link_latency
                new_event_time += link_latency
                new_dropped = not sender.path[next_hop].packet_enters_link(self.cur_time)
                push_new_event = True
                    
            if push_new_event:
                heapq.heappush(self.q, (new_event_time, sender, new_event_type, new_next_hop, new_latency, new_dropped))

        sender_mi = self.senders[0].get_run_data()
        throughput = sender_mi.recv_rate
        latency = sender_mi.latency
        loss = sender_mi.loss
        bw_cutoff = self.links[0].bw * 0.8 * RATE_OBS_SCALE
        lat_cutoff = 2.0 * self.links[0].dl * 1.5 * LAT_OBS_SCALE
        #print("thpt %f, bw %f" % (throughput, bw_cutoff))
        #reward = 0 if (loss > 0.1 or throughput < bw_cutoff or latency > lat_cutoff) else 1 #
        reward = REWARD_SCALE * (5.0 * throughput / RATE_OBS_SCALE - 1e3 * latency / LAT_OBS_SCALE - 2e3 * loss)
        #if reward > 857:
        #print("Reward = %f, thpt = %f, lat = %f, loss = %f" % (reward, throughput, latency, loss))
        return reward

class Sender():
    
    def __init__(self, rate, path, dest, history_len=1):
        self.starting_rate = rate
        self.rate = rate
        self.sent = 0
        self.acked = 0
        self.lost = 0
        self.bytes_in_flight = 0
        self.min_latency = None
        self.latency_samples = []
        self.sample_time = []
        self.net = None
        self.path = path
        self.dest = dest
        self.history_len = history_len
        self.history = SenderHistory(history_len)

    def apply_rate_delta(self, delta):
        delta *= DELTA_SCALE
        #print("Applying delta %f" % delta)
        if delta >= 0.0:
            self.set_rate(self.rate * (1.0 + delta))
        else:
            self.set_rate(self.rate / (1.0 - delta))

    def register_network(self, net):
        self.net = net

    def on_packet_sent(self):
        self.sent += 1
        self.bytes_in_flight += BYTES_PER_PACKET

    def on_packet_acked(self, latency):
        self.acked += 1
        self.latency_samples.append(latency)
        if (self.min_latency is None) or (latency < self.min_latency):
            self.min_latency = latency
        self.bytes_in_flight -= BYTES_PER_PACKET

    def on_packet_lost(self):
        self.lost += 1
        self.bytes_in_flight -= BYTES_PER_PACKET

    def set_rate(self, new_rate):
        self.rate = new_rate
        #print("Attempt to set new rate to %f (min %f, max %f)" % (new_rate, MIN_RATE, MAX_RATE))
        if self.rate > MAX_RATE:
            self.rate = MAX_RATE
        if self.rate < MIN_RATE:
            self.rate = MIN_RATE

    def record_run(self):
        smi = self.get_run_data()
        self.history.step(smi)

    def get_obs(self):
        return self.history.as_array()

    def get_run_data(self):
        obs_end_time = self.net.get_cur_time()
        obs_dur = obs_end_time - self.obs_start_time
        recv_rate = 0
        if obs_dur > 0:
            recv_rate = self.acked / obs_dur
        loss = 0
        if self.lost + self.acked > 0:
            loss = self.lost / (self.lost + self.acked)
 
        latency_inflation = 0.0
        avg_latency = 0.0
        latency_ratio = 1.0
        if len(self.latency_samples) > 0:
            latency_inflation = (self.latency_samples[-1] - self.latency_samples[0]) / (obs_end_time - self.obs_start_time)
            avg_latency = sum(self.latency_samples) / len(self.latency_samples)
            latency_ratio = avg_latency / self.min_latency

        send_ratio = 1000.0
        if (self.rate < 1000.0 * recv_rate) and (recv_rate > 0.0):
            send_ratio = self.rate / recv_rate

        #print("Got %d acks in %f seconds" % (self.acked, obs_dur))
        #print("Sent %d packets in %f seconds" % (self.sent, obs_dur))
 
        #print("self.rate = %f" % self.rate)
        return SenderMonitorInterval(self.rate * RATE_OBS_SCALE,
                    recv_rate * RATE_OBS_SCALE,
                    avg_latency * LAT_OBS_SCALE,
                    loss,
                    latency_inflation,
                    latency_ratio,
                    send_ratio)

    def reset_obs(self):
        self.sent = 0
        self.acked = 0
        self.lost = 0
        self.latency_samples = []
        self.obs_start_time = self.net.get_cur_time()

    def print_debug(self):
        print("Sender:")
        print("Obs: %s" % str(self.get_obs()))
        print("Rate: %f" % self.rate)
        print("Sent: %d" % self.sent)
        print("Acked: %d" % self.acked)
        print("Lost: %d" % self.lost)
        print("Min Latency: %s" % str(self.min_latency))

    def reset(self):
        #print("Resetting sender!")
        self.rate = self.starting_rate
        self.bytes_in_flight = 0
        self.min_latency = None
        self.reset_obs()
        self.history = SenderHistory(self.history_len)

class SimulatedNetworkEnv(gym.Env):
    
    def __init__(self, history_len=10):
        self.viewer = None
        self.rand = None

        self.min_bw, self.max_bw = (100, 500)
        self.min_lat, self.max_lat = (0.05, 0.5)
        self.min_queue, self.max_queue = (0, 8)
        self.min_loss, self.max_loss = (0.0, 0.05)
        self.history_len = history_len

        self.links = None
        self.senders = None
        self.create_new_links_and_senders()
        self.net = Network(self.senders, self.links)
        self.run_dur = None
        self.run_period = 0.1
        self.steps_taken = 0
        self.max_steps = 400
        self.debug_thpt_changes = False
        self.last_thpt = None
        self.last_rate = None

        self.action_space = spaces.Box(np.array([-1e12]), np.array([1e12]), dtype=np.float32)
       
        self.observation_space = None
        use_only_scale_free = True 
        if use_only_scale_free:
            self.observation_space = spaces.Box(np.tile(np.array([-1.0, 1.0, 0.0]), self.history_len),
                np.tile(np.array([10.0, 100.0, 1000.0]), self.history_len), dtype=np.float32) 
        else:
            self.observation_space = spaces.Box(np.array([10.0, 0.0, 0.0, 0.0, -100.0]),
                np.array([2000.0, 2000.0, 10.0, 1.0, 10.0]), dtype=np.float32) 

        self.reward_sum = 0.0
        self.reward_ewma = 0.0

        self.event_record = {"Events":[]}
        self.episodes_run = -1

    def seed(self, seed=None):
        self.rand, seed = seeding.np_random(seed)
        return [seed]

    def _get_all_sender_obs(self):
        sender_obs = self.senders[0].get_obs()
        sender_obs = np.array(sender_obs).reshape(-1,)
        return sender_obs

    def step(self, actions):
        #print("Actions: %s" % str(actions))
        for i in range(0, len(actions)):
            #print("Updating rate for sender %d" % i)
            self.senders[i].apply_rate_delta(actions[i])
        #print("Running for %fs" % self.run_dur)
        reward = self.net.run_for_dur(self.run_dur)
        for sender in self.senders:
            sender.record_run()
        self.steps_taken += 1
        sender_obs = self._get_all_sender_obs()
        sender_mi = self.senders[0].get_run_data()
        event = {}
        event["Name"] = "Step"
        event["Time"] = self.steps_taken
        event["Reward"] = reward
        event["Send Rate"] = sender_mi.rate
        event["Throughput"] = sender_mi.recv_rate
        event["Latency"] = sender_mi.latency
        event["Loss Rate"] = sender_mi.loss
        event["Latency Inflation"] = sender_mi.lat_infl
        event["Latency Ratio"] = sender_mi.lat_ratio
        event["Send Ratio"] = sender_mi.send_ratio
        self.event_record["Events"].append(event)
        if event["Latency"] > 0.0:
            self.run_dur = 0.5 * sender_mi.latency / LAT_OBS_SCALE
        #print("Sender obs: %s" % sender_obs)

        thpt_change = 0
        if self.last_thpt is not None and self.last_thpt > 0:
            thpt_change = np.abs((sender_mi.recv_rate - self.last_thpt) / self.last_thpt)
       
        if thpt_change > 0.5 and self.debug_thpt_changes:
            print("---Large throughput change event!---")
            print("Throughput change: %f -> %f" % (self.last_thpt, sender_mi.recv_rate))
            print("Rate change: %f -> %f" % (self.last_rate, sender_mi.rate))
            self.print_debug()
        self.last_thpt = sender_mi.recv_rate
        self.last_rate = sender_mi.rate

        should_stop = False #sender_obs[0] < 0.001 * 40.0 or sender_obs[2] > 0.1 * 0.12 or sender_obs[3] > 0.1 

        self.reward_sum += reward
        return sender_obs, reward, (self.steps_taken >= self.max_steps or should_stop), {}

    def print_debug(self):
        print("---Link Debug---")
        for link in self.links:
            link.print_debug()
        print("---Sender Debug---")
        for sender in self.senders:
            sender.print_debug()

    def create_new_links_and_senders(self):
        bw    = random.uniform(self.min_bw, self.max_bw)
        lat   = random.uniform(self.min_lat, self.max_lat)
        queue = 1 + int(np.exp(random.uniform(self.min_queue, self.max_queue)))
        #loss  = random.uniform(self.min_loss, self.max_loss)
        #bw    = 200
        #lat   = 0.03
        #queue = 5
        loss  = 0.00
        self.links = [Link(bw, lat, queue, loss), Link(bw, lat, queue, loss)]
        #self.senders = [Sender(0.3 * bw, [self.links[0], self.links[1]], 0)]
        self.senders = [Sender(random.uniform(0.3, 1.5) * bw, [self.links[0], self.links[1]], 0,
            self.history_len)]
        self.run_dur = 3 * lat

    def reset(self):
        self.steps_taken = 0
        self.net.reset()
        self.create_new_links_and_senders()
        self.net = Network(self.senders, self.links)
        self.episodes_run += 1
        if self.episodes_run > 0 and self.episodes_run % 100 == 0:
            self.dump_events_to_file("pcc_env_log_run_%d.json" % self.episodes_run)
        self.event_record = {"Events":[]}
        self.net.run_for_dur(self.run_dur)
        self.reward_ewma *= 0.99
        self.reward_ewma += 0.01 * self.reward_sum
        print("Reward: %0.2f, Ewma Reward: %0.2f" % (self.reward_sum, self.reward_ewma))
        self.reward_sum = 0.0
        return self._get_all_sender_obs()

    def render(self, mode='human'):
        pass

    def close(self):
        if self.viewer:
            self.viewer.close()
            self.viewer = None

    def dump_events_to_file(self, filename):
        with open(filename, 'w') as f:
            json.dump(self.event_record, f, indent=4)

register(id='PccNs-v0', entry_point='network_sim:SimulatedNetworkEnv')
#env = SimulatedNetworkEnv()
#env.step([1.0])
