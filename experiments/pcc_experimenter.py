import os
import sys
import pcc_remote_funcs
import pcc_experiment

simple = pcc_experiment.PccExperiment("Simple", 1, 100, 30, 75, 0.0, "-DEBUG_RATE_CONTROL -DEBUG_UTILITY_CALC -LOG_RATE_CONTROL_PARAMS")

user_name = sys.argv[1]
experiment_name = sys.argv[2]
experiment_names = sys.argv[3].split(" ")
replicas = int(sys.argv[4])
experiments = []
for name in experiment_names:
    experiments.append(pcc_experiment.PccExperiment.get_by_name(name))

senders = ["sender1"]
receivers = ["receiver1"]

def get_full_hostname(partial_name):
    global user_name
    global experiment_name
    return user_name + "@" + partial_name + "." + experiment_name + ".UIUCScheduling.emulab.net"

for i in range(0, len(senders)):
    senders[i] = get_full_hostname(senders[i])

for i in range(0, len(receivers)):
    receivers[i] = get_full_hostname(receivers[i])

remote_hosts = senders + receivers

for remote_host in remote_hosts:
    if (len(sys.argv) < 6 or sys.argv[5] != "--no-prepare"):
        pcc_remote_funcs.prepare_node(remote_host)

for experiment in experiments:
    for i in range(0, replicas):
        experiment.run(senders, receivers, get_full_hostname("bridge0"))

pcc_remote_funcs.call("mkdir results")
for sender in senders:
    remote_dir = "/tmp/local_pcc"
    pcc_remote_funcs.call("scp -r " + sender + ":" + remote_dir + "/pcc_log_* ./results/")
