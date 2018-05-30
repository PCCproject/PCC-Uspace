import sys
import os
import time
import random

n_workers = 2
replicas = 5

emulab_initialized = False

EXPERIMENT_DUR = 60

user = "njay2"
experiment_name = "njay-worker"
project = "UIUCScheduling"

def get_worker_name(worker_id):
    return experiment_name+"-" + str(worker_id)

def init_emulab_on_worker(worker_id, background):
    init_emulab_cmd = "./run_experiment_pcc.py -u "+user+" -e " + get_worker_name(worker_id) + " -t 1 -n 2"
    if (background):
        init_emulab_cmd += " &"
    os.system(init_emulab_cmd)

def init_emulab():
    for i in range(1, n_workers):
        init_emulab_on_worker(i, True)
    init_emulab_on_worker(n_workers, False)
    time.sleep(5)

def remove_old_results_from_worker(worker_name):
    hostname = user+"@sender1." + worker_name + "."+project+".emulab.net"
    os.system("ssh " + hostname + " \"rm /tmp/pcc_expr/pcc_log*\"")
    hostname = user+"@sender2." + worker_name + "."+project+".emulab.net"
    os.system("ssh " + hostname + " \"rm /tmp/pcc_expr/pcc_log*\"")

def get_results_from_worker(worker_name):
    os.system("mkdir -p ~/pcc_expr/results")
    hostname = user+"@sender1." + worker_name + "."+project+".emulab.net"
    os.system("scp " + hostname + ":/tmp/pcc_expr/pcc_log* ~/pcc_expr/results/")
    hostname = user+"@sender2." + worker_name + "."+project+".emulab.net"
    os.system("scp " + hostname + ":/tmp/pcc_expr/pcc_log* ~/pcc_expr/results/")

class ExperimentBatch:
    def __init__(self):
        self.experiments = []

    def add_experiment(self, experiment):
        self.experiments.append(experiment)

    def run(self):
        global emulab_initialized
        if not emulab_initialized:
            init_emulab()
            emulab_initialized = True
        for i in range(1, n_workers + 1):
            remove_old_results_from_worker(get_worker_name(i))
        cur_expr = 0
        while cur_expr < len(self.experiments):
            for i in range(1, n_workers + 1):
                if cur_expr < len(self.experiments):
                    self.experiments[cur_expr].run(get_worker_name(i), (not i == n_workers) and (not cur_expr == len(self.experiments) - 1))
                    cur_expr += 1
                    time.sleep(2)
            print "\t\t=================="
            print "\tFinished " + str(cur_expr) + "/ " + str(len(self.experiments)) + " experiments."
            print "\t\t=================="
        for i in range(1, n_workers + 1):
            get_results_from_worker(get_worker_name(i))


class ExperimentType:
    def __init__(self, thpt, lat, buf, loss, pairs, reps, attrs={}):
        self.attrs = attrs
        self.thpt = thpt
        self.lat = lat
        self.buf = buf
        self.loss = loss
        self.reps = reps
        self.pairs = pairs

    def run(self, worker_name, background):
        run_experiment_cmd = "./run_experiment_pcc.py -u "+user+" -e " + worker_name + " -n " + str(self.pairs) + " -t " + str(EXPERIMENT_DUR) + " -lb " + str(self.thpt) + " -rb " + str(self.thpt) + " -ld " + str(self.lat) + " -rd " + str(self.lat) + " -lq " + str(self.buf) + " -rq " + str(self.buf) + " -ll " + str(self.loss) + " -rl " + str(self.loss) + " -r " + str(self.reps) + " -ski -skc -skb "
        if len(self.attrs.keys()) > 0:
            run_experiment_cmd += " -a \" "
            for k in self.attrs.keys():
                run_experiment_cmd += k + str(self.attrs[k]) + " "
            run_experiment_cmd += "\""
        if background:
            run_experiment_cmd += " &"
        print run_experiment_cmd
        os.system(run_experiment_cmd)

param_sets = [
{"--vivace-latency-utility":""}
]

class LinkConfiguration:
    def __init__(self, bandwidth, latency, buffer_size, loss_rate):
        self.bw = bandwidth
        self.lat = latency
        self.buf = buffer_size
        self.lr = loss_rate

link_configs = []

exp_list = []
for param_set in param_sets:
    for i in range(0, replicas):
        loss_rates = [0.0, 0.005, 0.01, 0.02, 0.03, 0.05, 0.15]
        attrs = param_set.copy()
        attrs["-LOSS_EXPR"] = ""
        for loss_rate in loss_rates:
            exp = ExperimentType(100, 30, 75, loss_rate, 1, 1, attrs)
            exp_list.append(exp)

        bufs = [2, 8, 100, 1000, 200000]
        attrs = param_set.copy()
        attrs["-BUFFER_EXPR"] = ""
        for buf in bufs:
            exp = ExperimentType(100, 30, buf, 0.0, 1, 1, attrs)
            exp_list.append(exp)
        
        bufs = [2, 8, 100, 1000, 200000]
        attrs = param_set.copy()
        attrs["-SAT_EXPR"] = ""
        for buf in bufs:
            exp = ExperimentType(42, 800, buf, 0.074, 1, 1, attrs)
            exp_list.append(exp)

        lats = [2, 5, 15, 30, 100, 400]
        attrs = param_set.copy()
        attrs["-LATENCY_EXPR"] = ""
        for lat in lats:
            exp = ExperimentType(100, lat, 75, 0.0, 1, 1, attrs)
            exp_list.append(exp)
       
        attrs = param_set.copy()
        attrs["-CONVERGENCE_EXPR"] = ""
        exp = ExperimentType(100, 30, 750, 0.0, 2, 1, attrs)
        exp_list.append(exp)

experiment_list = ExperimentBatch()
for exp in exp_list:
    experiment_list.add_experiment(exp)
experiment_list.run()

