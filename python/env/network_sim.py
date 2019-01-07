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
MIN_RATE = 20

DELTA_SCALE = 0.1

EVENT_TYPE_SEND = 'S'
EVENT_TYPE_ACK = 'A'

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
        extra_delay = 1.0 / self.bw
        if extra_delay + self.queue_delay > self.max_queue_delay:
            #print("\tDrop!")
            return False
        self.queue_delay += extra_delay
        self.queue_delay_update_time = event_time
        #print("\tNew delay = %f" % self.queue_delay)
        return True

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
                if next_hop == -1:
                    if dropped:
                        sender.on_packet_lost()
                    else:
                        sender.on_packet_acked(cur_latency)
                else:
                    new_next_hop = next_hop - 1
                    link_latency = sender.path[next_hop].get_cur_latency(self.cur_time)
                    new_latency += link_latency
                    new_event_time += link_latency
                    push_new_event = True
            if event_type == EVENT_TYPE_SEND:
                if next_hop == 0:
                    sender.on_packet_sent()
                    heapq.heappush(self.q, (self.cur_time + (1.0 / sender.rate), sender, EVENT_TYPE_SEND, 0, 0.0, False))

                if next_hop == len(sender.path) - 1:
                    new_event_type = EVENT_TYPE_ACK
                    new_next_hop = next_hop
                else:
                    new_next_hop = next_hop + 1
                
                link_latency = sender.path[next_hop].get_cur_latency(self.cur_time)
                new_latency += link_latency
                new_event_time += link_latency
                new_dropped = not sender.path[next_hop].packet_enters_link(self.cur_time)
                push_new_event = True
                    
            if push_new_event:
                heapq.heappush(self.q, (new_event_time, sender, new_event_type, new_next_hop, new_latency, new_dropped))

        obs = [sender.get_obs() for sender in self.senders]
        throughput = sum([ob[2] for ob in obs]) / float(dur)
        latency = sum([ob[4] for ob in obs]) / len(obs)
        loss = sum([ob[3] for ob in obs]) / float(dur)
        reward = throughput - 1e3 * latency - 10 * loss
        if reward > 857:
            print("Reward = %f, thpt = %f, lat = %f, loss = %f" % (reward, throughput, latency, loss))
        return reward

class Sender():
    
    def __init__(self, rate, path):
        self.starting_rate = rate
        self.rate = rate
        self.sent = 0
        self.acked = 0
        self.lost = 0
        self.latency_samples = []
        self.sample_time = []
        self.net = None
        self.path = path

    def apply_rate_delta(self, delta):
        delta *= DELTA_SCALE
        if delta >= 0.0:
            self.set_rate(self.rate * (1.0 + delta))
        else:
            self.set_rate(self.rate / (1.0 - delta))

    def register_network(self, net):
        self.net = net

    def on_packet_sent(self):
        self.sent += 1

    def on_packet_acked(self, latency):
        self.acked += 1
        self.latency_samples.append(latency)

    def on_packet_lost(self):
        self.lost += 1

    def set_rate(self, new_rate):
        self.rate = new_rate
        #print("Attempt to set new rate to %f (min %f, max %f)" % (new_rate, MIN_RATE, MAX_RATE))
        if self.rate > MAX_RATE:
            self.rate = MAX_RATE
        if self.rate < MIN_RATE:
            self.rate = MIN_RATE

    def get_obs(self):
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
        if len(self.latency_samples) > 0:
            (self.latency_samples[-1] - self.latency_samples[0]) / (obs_end_time - self.obs_start_time)
            avg_latency = sum(self.latency_samples) / len(self.latency_samples)
        
        #print("self.rate = %f" % self.rate)
        return [self.rate,
                recv_rate,
                avg_latency,
                loss,
                latency_inflation]

    def reset_obs(self):
        self.sent = 0
        self.acked = 0
        self.lost = 0
        self.latency_samples = []
        self.obs_start_time = self.net.get_cur_time()

    def reset(self):
        self.rate = self.starting_rate
        self.reset_obs()

class SimulatedNetworkEnv(gym.Env):
    
    def __init__(self):
        self.viewer = None
        self.rand = None

        self.min_bw, self.max_bw = (200, 600)
        self.min_lat, self.max_lat = (0.01, 0.1)
        self.min_queue, self.max_queue = (5, 500)
        self.min_loss, self.max_loss = (0.0, 0.0)

        self.links = None
        self.senders = None
        self.create_new_links_and_senders()
        self.net = Network(self.senders, self.links)
        self.run_period = 0.1
        self.steps_taken = 0
        self.max_steps = 200

        self.action_space = spaces.Box(np.array([-1e12]), np.array([1e12]), dtype=np.float32)
        self.observation_space = spaces.Box(np.array([10.0, 0.0, 0.0, 0.0, -100.0, -10000.0]),
            np.array([2000.0, 2000.0, 10.0, 1.0, 10.0, 2000.0]), dtype=np.float32) 

        self.event_record = {"Events":[]}
        self.episodes_run = -1

    def seed(self, seed=None):
        self.rand, seed = seeding.np_random(seed)
        return [seed]

    def _get_all_sender_obs(self, reward):
        sender_obs = self.senders[0].get_obs()
        sender_obs.append(reward)
        sender_obs = np.array(sender_obs).reshape(-1,)
        return sender_obs

    def step(self, actions):
        #print("Actions: %s" % str(actions))
        for i in range(0, len(actions)):
            #print("Updating rate for sender %d" % i)
            self.senders[i].apply_rate_delta(actions[i])
        reward = self.net.run_for_dur(self.run_period)
        self.steps_taken += 1
        sender_obs = self._get_all_sender_obs(reward)
        event = {}
        event["Name"] = "Step"
        event["Time"] = self.steps_taken
        event["Reward"] = reward
        """
        event["Send Rate"] = sender_obs[0][0]
        event["Throughput"] = sender_obs[1][0]
        event["Latency"] = sender_obs[2][0]
        event["Loss Rate"] = sender_obs[3][0]
        event["Latency Inflation"] = sender_obs[4][0]
        """
        self.event_record["Events"].append(event)
        #print("Sender obs: %s" % sender_obs)

        return sender_obs, reward, self.steps_taken >= self.max_steps, {}

    def create_new_links_and_senders(self):
        bw    = random.uniform(self.min_bw, self.max_bw)
        lat   = random.uniform(self.min_lat, self.max_lat)
        queue = random.uniform(self.min_queue, self.max_queue)
        loss  = random.uniform(self.min_loss, self.max_loss)
        self.links = [Link(bw, lat, queue, loss)]
        self.senders = [Sender(random.uniform(0.2, 1.5) * bw, [self.links[0]])]

    def reset(self):
        print("Reset called!")
        self.steps_taken = 0
        self.net.reset()
        self.create_new_links_and_senders()
        self.net = Network(self.senders, self.links)
        self.episodes_run += 1
        if self.episodes_run > 0 and self.episodes_run % 100 == 0:
            self.dump_events_to_file("pcc_env_log_run_%d.json" % self.episodes_run)
        self.event_record = {"Events":[]}
        return self._get_all_sender_obs(0.0)

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
