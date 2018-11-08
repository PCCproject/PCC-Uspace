import os
import Queue
import multiprocessing
import time
import random


rates = []
with open("speeds.txt") as speeds:
    for speed in speeds:
        rates.append(int(speed))

rates = range(16, 64)

################################################################################
def get_hostname(node_id, username, expr, proj) :
  return username + "@" + node_id + "." + expr + "." + proj + ".emulab.net"
################################################################################


################################################################################
def remote_call(remote_host, cmd) :
  print ("ssh " + remote_host + " \"" + cmd + "\"")
  os.system("ssh -o StrictHostKeyChecking=no " + remote_host + " \"" + cmd + "\"")
################################################################################


################################################################################
def remote_call_background(remote_host, cmd) :
  print ("ssh " + remote_host + " \"" + cmd + "\" &")
  os.system("ssh -o StrictHostKeyChecking=no " + remote_host + " \"" + cmd + "\" &")
################################################################################


################################################################################
def remote_copy(path1, path2) :
  print ("scp -r " + path1 + " " + path2)
  os.system("scp -r " + path1 + " " + path2)
################################################################################

sender_name = get_hostname("sender1", "njay2", "njay-worker-lite-1", "UIUCScheduling")
bridge_name = get_hostname("bridge0", "njay2", "njay-worker-lite-1", "UIUCScheduling")
receiver_name = get_hostname("receiver1", "njay2", "njay-worker-lite-1", "UIUCScheduling")
scheme = "pcc_experimental"
local_results_dir = "/home/njay2/pcc_results/"
remote_pantheon_dir = "/users/njay2/pantheon/"
remote_results_dir = "/tmp/"

def run_changing_link(bridge, dl, buf, plr):
    done = False
    random.seed(0)
    while not done:
        new_bw = rates[random.randint(1, len(rates) - 1)]
        if (new_bw < 1):
            new_bw = 1
        remote_call(bridge, "sudo ipfw pipe 100 config bw %dMbit/s delay %dms queue %dkB plr %3f" % (new_bw, dl, buf, plr))
        
        time.sleep(5)

def run_bridge_setup(bridge, bw, dl, buf, plr):
    remote_call(bridge, "sudo ipfw delete 100")
    remote_call(bridge, "sudo ipfw pipe 100 config bw %dMbit/s delay %dms queue %dkB plr %3f" % (bw, dl, buf, plr))
    remote_call(bridge, "sudo ipfw add 100 pipe 100 all from any to 10.1.1.3")

bridge = "njay2@bridge0.njay-worker-lite-1.UIUCScheduling.emulab.net"
run_bridge_setup(bridge, 6, 32, 50, 0.0)
run_changing_link(bridge, 32, 50, 0.0)
