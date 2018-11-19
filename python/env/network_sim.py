import gym
from gym import spaces
from gym.utils import seeding
from gym.envs.registration import register
import numpy as np
import heapq
import time

MAX_RATE = 1000
MIN_RATE = 20

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
        #print("Packet enters link")
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
        if self.rate > MAX_RATE:
            self.rate = MAX_RATE
        if self.rate < MIN_RATE:
            self.rate = MIN_RATE

    def get_obs(self):
        obs_end_time = self.net.get_cur_time()
        latency_inflation = 0.0
        avg_latency = 0.0
        if len(self.latency_samples) > 0:
            (self.latency_samples[-1] - self.latency_samples[0]) / (obs_end_time - self.obs_start_time)
            avg_latency = sum(self.latency_samples) / len(self.latency_samples)
        return [self.rate,
                self.sent,
                self.acked,
                self.lost,
                avg_latency,
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
        self.viwer = None
        self.rand = None
        self.links = [Link(300.0, 0.03, 100, 0.0)]
        self.senders = [Sender(330.0, [self.links[0]])]
        self.net = Network(self.senders, self.links)
        self.run_period = 0.1
        self.steps_taken = 0
        self.max_steps = 200

        self.action_space = spaces.Box(np.array([-1e12]), np.array([1e12]), dtype=np.float32)
        self.observation_space = spaces.Box(np.array([10.0, 0.0, 0.0, 0.0, 0.0, -100.0]),
            np.array([2000.0, 200.0, 200.0, 200.0, 1.0, 100.0]), dtype=np.float32) 

    def seed(self, seed=None):
        self.rand, seed = seeding.np_random(seed)
        return [seed]

    def step(self, actions):
        for i in range(0, len(actions)):
            self.senders[i].set_rate(self.senders[i].rate * actions[i])
        reward = self.net.run_for_dur(self.run_period)
        self.steps_taken += 1
        return np.array(self.senders[0].get_obs()), reward, self.steps_taken >= self.max_steps, {}

    def reset(self):
        self.steps_taken = 0
        self.net.reset()
        return np.array(self.senders[0].get_obs())

    def render(self, mode='human'):
        pass

    def close(self):
        if self.viewer:
            self.viewer.close()
            self.viewer = None

register(id='PccNs-v0', entry_point='network_sim:SimulatedNetworkEnv')
#env = SimulatedNetworkEnv()
#env.step([1.0])
