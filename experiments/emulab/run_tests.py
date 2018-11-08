import os
import Queue
import multiprocessing
import time
import random

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

worker_name = "njay-worker-lite-1"
sender_name = get_hostname("sender1", "njay2", worker_name, "UIUCScheduling")
bridge_name = get_hostname("bridge0", "njay2", worker_name, "UIUCScheduling")
receiver_name = get_hostname("receiver1", "njay2", worker_name, "UIUCScheduling")
scheme = "pcc_experimental"
local_results_dir = "/home/njay2/pcc_results/"
remote_pantheon_dir = "/users/njay2/pantheon/"
remote_results_dir = "/tmp/"

all_schemes = [
#"pcc_rl_loss",
#"pcc_rl_copa",
#"vivace_loss"
#"default_tcp",
#"copa",
#"vivace_latency",
#"bbr"#,
"pcc_experimental"
#"taova"
]

################################################################################
def install_dependencies(args) :
  # should run on Ubuntu node
  sender_cmds = []
  receiver_cmds = []

  sender_cmds.append("sudo apt-get update")
  sender_cmds.append("sudo apt-get -y --force-yes install vim curl software-properties-common iperf")
  sender_cmds.append("sudo add-apt-repository -y ppa:fkrull/deadsnakes")
  sender_cmds.append("sudo apt-get update")
  sender_cmds.append("sudo apt-get -y --force-yes install python3.5 python3.5-dev")
  sender_cmds.append("curl https://bootstrap.pypa.io/get-pip.py | sudo python3.5")
  sender_cmds.append("curl https://bootstrap.pypa.io/get-pip.py | sudo python")
  sender_cmds.append("sudo pip install pyyaml")
  sender_cmds.append("sudo python3.5 -m pip install numpy")
  sender_cmds.append("sudo apt-get -y install libblas-dev liblapack-dev libatlas-base-dev gfortran")
  sender_cmds.append("sudo python3.5 -m pip install --upgrade pip")
  sender_cmds.append("sudo python3.5 -m pip install scipy gym tensorflow==1.5")
  sender_cmds.append("sudo apt-get -y --force-yes install libfreetype6-dev pkg-config libpng12-dev")
  sender_cmds.append("%s/install_deps.sh" % remote_pantheon_dir)
  for scheme in all_schemes:
      sender_cmds.append("%s/test/setup.py --install-deps --schemes %s" % (remote_pantheon_dir, scheme))

  receiver_cmds.append("sudo apt-get update")
  receiver_cmds.append("sudo apt-get -y --force-yes install vim curl software-properties-common")
  receiver_cmds.append("sudo add-apt-repository -y ppa:fkrull/deadsnakes")
  receiver_cmds.append("curl https://bootstrap.pypa.io/get-pip.py | sudo python")
  receiver_cmds.append("sudo pip install pyyaml")
  receiver_cmds.append("sudo apt-get update")
  receiver_cmds.append("sudo apt-get -y --force-yes install python3.5")
  receiver_cmds.append("sudo apt-get -y --force-yes install python3.5-dev")
  receiver_cmds.append("sudo apt -y install iperf")
  receiver_cmds.append("sudo apt-get -y --force-yes install libfreetype6-dev pkg-config libpng12-dev")
  receiver_cmds.append("%s/install_deps.sh" % remote_pantheon_dir)
  for scheme in all_schemes:
      receiver_cmds.append("%s/test/setup.py --install-deps --schemes %s" % (remote_pantheon_dir, scheme))
  
  for cmd in sender_cmds:
      remote_call(sender_name, cmd)
  for cmd in receiver_cmds:
      remote_call(receiver_name, cmd)
  """
  for i in range(1, args.n + 1) :
    remote_sender = get_hostname("sender" + str(i), args.u, args.e, args.p)
    remote_receiver = get_hostname("receiver" + str(i), args.u, args.e, args.p)
    for cmd in sender_cmds:
      remote_call(remote_sender, cmd)
    for cmd in receiver_cmds:
      remote_call(remote_receiver, cmd)
  """
################################################################################

def run_changing_link(done_queue, bridge, cfg):
    done = False
    random.seed(0)
    while not done:
        new_bw = random.randint(cfg.bw_min, cfg.bw_max + 1)
        print("Changing link bandwidth to %d" % new_bw)
        remote_call(bridge, "sudo ipfw pipe 100 config bw %dMbit/s delay %dms queue %dkB plr %3f" % (new_bw, cfg.dl, cfg.buf, cfg.plr))
        time.sleep(cfg.interval)
        try:
            done_queue.get(block=False)
            done = True
        except Queue.Empty:
            pass

class LinkConfig:
    def __init__(self, bw, dl, buf, plr, flow_count):
        self.bw = bw
        self.dl = dl
        self.buf = buf
        self.plr = plr
        self.flow_count = flow_count

        self.bw_min = None
        self.bw_max = None
        self.interval = None

        self.child = None
        self.child_queue = None

    def add_bandwidth_range(self, bw_min, bw_max, interval):
        self.bw_min = bw_min
        self.bw_max = bw_max
        self.interval = interval
        self.child_queue = multiprocessing.Queue()

    def run_bridge_setup(self, bridge):
        remote_call(bridge, "sudo ipfw delete 100")
        remote_call(bridge, "sudo ipfw pipe 100 config bw %dMbit/s delay %dms queue %dkB plr %3f" % (self.bw, self.dl, self.buf, self.plr))
        remote_call(bridge, "sudo ipfw add 100 pipe 100 all from any to 10.1.1.3")
        if self.interval is not None:
            self.child = multiprocessing.Process(target=run_changing_link,
                args=(self.child_queue, bridge, self))
            self.child.start()

    def get_test_name(self):
        if self.interval is None:
            return "test_bw%d_dl%d_buf%d_plr%f_f%d" % (self.bw, self.dl, self.buf, self.plr, self.flow_count)
        else:
            return "test_bw%d_to%d_dl%d_buf%d_plr%f_f%d" % (self.bw_min, self.bw_max, self.dl, self.buf, self.plr,
            self.flow_count)

    def reset_bridge(self, bridge):
        if (self.interval is not None):
            self.child_queue.put("stop")
            self.child.join()

def run_test(scheme, link_cfg, test_dur, n_replicas):
    base_run_id = 1
    for run_id in range(base_run_id, n_replicas + base_run_id):
        test_name = link_cfg.get_test_name()
        os.system("mkdir %s%s" % (local_results_dir, test_name))
        link_cfg.run_bridge_setup(bridge_name)
        flow_interval = test_dur / link_cfg.flow_count
        flow_interval = 0
        cmd = "%stest/test.py remote --schemes %s --data-dir %s -t %d --flows %d --interval %d 10.1.1.3:%s" % (remote_pantheon_dir, scheme, remote_results_dir, test_dur, link_cfg.flow_count, flow_interval, remote_pantheon_dir)
        print(cmd)
        remote_call(sender_name, cmd)
        link_cfg.reset_bridge(bridge_name)
        files_to_copy = ["acklink", "datalink", "stats"]
        for f in files_to_copy:
            remote_copy("%s:%s%s_%s_run1.log" %  (sender_name, remote_results_dir, scheme, f),
                        "%s%s/%s_%s_run%d.log" % (local_results_dir, test_name, scheme, f, run_id))
        remote_call(sender_name, "rm %s%s_*" % (remote_results_dir, scheme))

install_dependencies([])

link_cfgs = []

base_bw = 32
base_rtt = 32
base_buf = 500
base_plr = 0.0

bws = [2, 4, 8, 16, 32, 48, 64, 72, 80, 96, 128]
dls = [2, 4, 8, 16, 48, 64, 72, 80, 96, 128]
bufs = [2, 5, 50, 150, 350, 1000, 5000]
plrs = [0.001, 0.01, 0.02, 0.05]
flow_counts = [2, 3, 4, 5, 6, 7, 8]

"""
for bw in bws:
    link_cfgs.append(LinkConfig(bw, base_rtt, base_buf, base_plr, 1))

for dl in dls:
    link_cfgs.append(LinkConfig(base_bw, dl, base_buf, base_plr, 1))

for buf in bufs:
    link_cfgs.append(LinkConfig(base_bw, base_rtt, buf, base_plr, 1))

for plr in plrs:
    link_cfgs.append(LinkConfig(base_bw, base_rtt, base_buf, plr, 1))

for flow_count in flow_counts:
    link_cfgs.append(LinkConfig(base_bw, base_rtt, base_buf, base_plr, flow_count))

cfg = LinkConfig(base_bw, base_rtt, base_buf, base_plr, 1)
cfg.add_bandwidth_range(16, 64, 5)
link_cfgs.append(cfg)
"""

for scheme in all_schemes:
    for link_cfg in link_cfgs:
        run_test(scheme, link_cfg, 30, 2)
