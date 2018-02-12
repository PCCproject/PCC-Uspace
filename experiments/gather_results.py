import sys
import os
import time

n_workers = 5
replicas = 3

def get_worker_name(worker_id):
    return "njay-worker-" + str(worker_id)

def get_results_from_worker(worker_name):
    hostname = "njay2@sender1." + worker_name + ".UIUCScheduling.emulab.net"
    os.system("mkdir -p ~/pcc_expr")
    os.system("mkdir -p ~/pcc_expr/results")
    os.system("scp " + hostname + ":/tmp/pcc_expr/pcc_log* ~/pcc_expr/results/")
    hostname = "njay2@sender2." + worker_name + ".UIUCScheduling.emulab.net"
    os.system("mkdir -p ~/pcc_expr")
    os.system("mkdir -p ~/pcc_expr/results")
    os.system("scp " + hostname + ":/tmp/pcc_expr/pcc_log* ~/pcc_expr/results/")

for i in range(1, 6):
    get_results_from_worker(get_worker_name(i))
