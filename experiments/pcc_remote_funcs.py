import os
import sys

def call(cmd):
    os.system(cmd)

def remote_call(remote_host, cmd):
    call("ssh " + remote_host + " \"" + cmd + "\"")

def remote_build_pcc(remote_host):
    remote_dir = "/tmp/local_pcc"
    lib_path = remote_dir + "/src/core"
    lib_path_env = "setenv LD_LIBRARY_PATH \\\"" + lib_path + "\\\""
    remote_call(remote_host, "mkdir -p " + remote_dir + "/src")
    copy_path = os.path.dirname(os.path.realpath(__file__)) + "/../"
    call("scp -r " + copy_path + "/* " + remote_host + ":" + remote_dir)
    remote_call(remote_host, "cd " + remote_dir + "/src" + " && make clean && make")
    remote_call(remote_host, "grep -q -F \'" + lib_path_env + "\' ~/.cshrc || echo \'" + lib_path_env + "\' >> ~/.cshrc")
    script_path = remote_dir + "/experiments/bash"
    remote_call(remote_host, script_path + "run_prepare.sh")

def remote_build_dependencies(remote_host):
    remote_call(remote_host, "sudo apt-get update")
    remote_call(remote_host, "sudo apt-get --assume-yes install python-dev python-pip vim iperf")
    remote_call(remote_host, "sudo -H pip install --upgrade pip")
    remote_call(remote_host, "sudo -H pip install --upgrade tensorflow")

#units are mbps, ms, KB and proportion [0, 1]
def run_bridge_setup(bridge_node, n_pairs, bandwidth, delay, queue_size, loss_rate):
    remote_dir = "/tmp/local_pcc"
    script_path = remote_dir + "/experiments/bash"
    remote_call(bridge_node, "sudo ipfw delete 100")
    remote_call(bridge_node, "sudo ipfw pipe 100 config bw " + str(bandwidth) + "Mbit/s delay " + str(delay) + "ms queue " + str(queue_size) + "KB plr " + str(loss_rate))
    for i in range(0, n_pairs):
        ip = "10.1.1." + str(i + 3)
        remote_call(bridge_node, "sudo ipfw add 100 pipe 100 all from any to " + ip)

def run_receiver(remote_host, args):
    remote_dir = "/tmp/local_pcc"
    receiver_executable = remote_dir + "/src/app/pccserver"
    script_path = remote_dir + "/experiments/bash"
    remote_call(remote_host, script_path + "/run_logged_program.sh " + receiver_executable + " \\\"" + args + "\\\" /dev/null /dev/null 0")

def run_sender(remote_host, args, duration):
    remote_dir = "/tmp/local_pcc"
    sender_executable = remote_dir + "/src/app/pccclient"
    script_path = remote_dir + "/experiments/bash"
    remote_call(remote_host, script_path + "/run_logged_program.sh " + sender_executable + " \\\"" + args + "\\\" /dev/null /dev/null " + str(duration))
    
def kill_receiver(remote_host):
    remote_call(remote_host, "sudo killall -s SIGINT pccserver")

def prepare_node(remote_host):
    remote_dir = "/tmp/local_pcc"
    script_path = remote_dir + "/experiments/bash"
    remote_build_dependencies(remote_host)
    remote_build_pcc(remote_host)
    remote_call(remote_host, script_path + "/run_prepare.sh")
