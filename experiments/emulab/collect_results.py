import sys
import os
import time
import random
from common.python import ExperimentalConfigs

###############################################################################
# Script configuration                                                        #
#                                                                             #
# Add a block to fit your own experimental setup. Uncomment as needed.        #
###############################################################################

#"""
n_workers = 8
nodes_per_worker = 1
user = "njay2"
worker_prefix = "njay-worker-lite-"
project = "UIUCScheduling"
#"""
#user = 
#worker_prefix = 
#project = 
#"""

def get_worker_name(cfg, worker_id):
    return cfg["worker prefix"] + str(worker_id)

def get_results_from_worker(cfg, worker_name):
    local_result_dir = cfg["local result dir"]
    remote_expr_dir = cfg["remote expr dir"]
    os.system("mkdir -p " + local_result_dir)
    for i in range(0, cfg["nodes per pair"]):
        hostname = "%s@sender%d.%s.%s.emulab.net" % 
            (cfg["user"], str(1 + i), worker_name, cfg["project name"])
        os.system("scp " + hostname + ":" + remote_expr_dir + "/pcc_log* " + local_result_dir)

cfg = ExperimentalConfigs.get_config(sys.argv)

for i in range(1, 2):
    get_results_from_worker(cfg, get_worker_name(cfg, i))
