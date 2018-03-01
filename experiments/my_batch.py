import sys
import os
import time
import random

n_configs = 0
n_workers = 8
replicas = 5

bw_max = 50
bw_min = 150

lat_max = 15
lat_min = 45

buf_max = 1075
buf_min = 375

lr_min = 0.0
lr_max = 0.02

emulab_initialized = False

EXPERIMENT_DUR = 3600

user = "nogar02"
experiment_name = "basic-test"
#project = "UIUCScheduling"
project = "LBCC"



def get_worker_name(worker_id):
    return experiment_name+"-" + str(worker_id)

def init_emulab_on_worker(worker_id, background):
    init_emulab_cmd = "./experiments/run_experiment_pcc.py -u "+user+" -e " + get_worker_name(worker_id) + " -t 1 -n 1"
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
        run_experiment_cmd = "./experiments/run_experiment_pcc.py -u "+user+" -e " + worker_name + " -n " + str(self.pairs) + " -t " + str(EXPERIMENT_DUR) + " -lb " + str(self.thpt) + " -rb " + str(self.thpt) + " -ld " + str(self.lat) + " -rd " + str(self.lat) + " -lq " + str(self.buf) + " -rq " + str(self.buf) + " -ll " + str(self.loss) + " -rl " + str(self.loss) + " -r " + str(self.reps) + " -ski -skc -skb "
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
{"--inverted-exponent-utility":"", "-pyhelper=":"pcc_gym_driver",
"-pypath=":"/tmp/pcc_expr/python/models/gym-16-hidden-width/"},
{"--inverted-exponent-utility":"", "-pyhelper=":"pcc_gym_driver",
"-pypath=":"/tmp/pcc_expr/python/models/gym-16-timesteps-per-batch/"},
{"--inverted-exponent-utility":"", "-pyhelper=":"pcc_gym_driver",
"-pypath=":"/tmp/pcc_expr/python/models/gym-1-hidden-layer/"},
{"--inverted-exponent-utility":"", "-pyhelper=":"pcc_gym_driver",
"-pypath=":"/tmp/pcc_expr/python/models/gym-256-timesteps-per-batch/"},
{"--inverted-exponent-utility":"", "-pyhelper=":"pcc_gym_driver",
"-pypath=":"/tmp/pcc_expr/python/models/gym-3-hidden-layers/"},
{"--inverted-exponent-utility":"", "-pyhelper=":"pcc_gym_driver",
"-pypath=":"/tmp/pcc_expr/python/models/gym-env/"},
{"--inverted-exponent-utility":"", "-pyhelper=":"pcc_gym_driver",
"-pypath=":"/tmp/pcc_expr/python/models/gym-relu/"}
#{"--inverted-exponent-utility":"", "-pyhelper=":"pcc_addon", "-pypath=":"/tmp/pcc_expr/src/", "--online-learning":""}#,
#{"--cubed-loss-utility":""},
#{"--inverted-exponent-utility":"", "--njay-ascent":""},
#{"--inverted-exponent-utility":""},
#{"-mdur=":"10"},
#{"--p-rand-rate=":"0.5"},
#{"--p-rand-rate=":"0.5", "--inverted-exponent-utility":""},
#{"-DEBUG_PCC_STATE_MACHINE":""},
#{"--lr-utility":""},
#{"--lr-utility":"", "--njay-ascent":""}#,
#{"--vivace-latency-utility":""}
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
    for c in range(0, n_configs):
        bw = int(random.uniform(bw_min, bw_max))
        lat = int(random.uniform(lat_min, lat_max))
        buf = int(random.uniform(buf_min, buf_max))
        lr = random.uniform(lr_min, lr_max)
        #                    thpt, lat, buf, loss, n-pairs, 1
        attrs = param_set.copy()
        attrs["-VARIATION_EXPR"] = ""
        #exp = ExperimentType(bw, lat, buf, lr, 1, 1, param_set)
        exp = ExperimentType(bw, lat, buf, 0, 1, 1, attrs)
        exp_list.append(exp)

    for i in range(0, replicas):
        mdurs = []#0.5, 0.1]
        for mdur in mdurs:
            attrs = param_set.copy()
            attrs["-mdur="] = mdur
            exp = ExperimentType(100, 10, 50, 0.0, 1, 1, attrs)
            #exp_list.append(exp)
        
        """
        loss_rates = [0.0, 0.01, 0.03, 0.05, 0.15]
        for loss_rate in loss_rates:
            exp = ExperimentType(100, 30, 75, loss_rate, 1, 1)
            #exp_list.append(exp)

        bufs = [2, 8, 100, 1000, 200000]
        for buf in bufs:
            exp = ExperimentType(100, 30, buf, 0.0, 1, 1)
            #exp_list.append(exp)

        lats = [2, 5, 15, 30, 100, 400]
        for lat in lats:
            exp = ExperimentType(100, lat, 75, 0.0, 1, 1)
            #exp_list.append(exp)
        """
       
        attrs = param_set.copy()
        attrs["-BASE_EXPR"] = ""
        exp = ExperimentType(100, 10, 50, 0.0, 1, 1, attrs)
        exp_list.append(exp)
        """ 
        attrs = param_set.copy()
        attrs["-CONVERGENCE_EXPR"] = ""
        exp = ExperimentType(100, 30, 750, 0.0, 2, 1, attrs)
        exp_list.append(exp)

        attrs = param_set.copy()
        attrs["-LEARNING_EXPR"] = ""
        attrs["--online-learning"] = ""
        exp = ExperimentType(100, 30, 750, 0.0, 2, 1, attrs)
        exp_list.append(exp)
        """

experiment_list = ExperimentBatch()

for exp in exp_list:
    experiment_list.add_experiment(exp)
experiment_list.run()

