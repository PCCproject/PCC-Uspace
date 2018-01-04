import os
import sys
import pcc_remote_funcs
import time

class PccExperiment:
    _experiments = {}

    def __init__(self, name, n_pairs, bandwidth, delay, queue, loss_rate, args):
        self.state = "WAITING"
        self.name = name
        self.n_pairs = n_pairs
        self.bandwidth = bandwidth
        self.delay = delay
        self.queue = queue
        self.loss_rate = loss_rate
        self.sender_args = "send 10.1.1.3 9000 " + args
        self.receiver_args = "recv 9000"
        PccExperiment._experiments[name] = self

    def run(self, senders, receivers, bridge):
        self.state = "RUNNING"
        if (len(senders) < self.n_pairs or len(receivers) < self.n_pairs):
            print "ERROR: PccExperiment: too few sender/reciever pairs available for experiment " + self.name
        print "PccExperimenter: running bridge setup..."
        pcc_remote_funcs.run_bridge_setup(bridge, self.n_pairs, self.bandwidth, self.delay, self.queue, self.loss_rate)
        print "PccExperimenter: running receiver..."
        pcc_remote_funcs.run_receiver(receivers[0], self.receiver_args)
        print "PccExperimenter: running sender..."
        time_ms = int(round(time.time() * 1000))
        pcc_remote_funcs.run_sender(senders[0], self.sender_args + " -log=/tmp/local_pcc/pcc_log_" + str(time_ms) + ".txt -experiment=" + self.name + " -npairs=" + str(self.n_pairs) + " -bandwidth=" + str(self.bandwidth) + " -delay=" + str(self.delay) + " -queue=" + str(self.queue) + " -loss=" + str(self.loss_rate), 10)
        pcc_remote_funcs.remote_call(receivers[0], "sudo killall pccserver")
        pcc_remote_funcs.remote_call(senders[0], "sudo killall pccserver")
        self.state = "FINISHED"
        print "PccExperimenter: done!"

    def stop(self):
        self.state = "STOPPED"

    def reset(self):
        self.state = "WAITING"

    def get_state(self):
        return self.state

    @staticmethod
    def get_by_name(name):
        return PccExperiment._experiments[name]
