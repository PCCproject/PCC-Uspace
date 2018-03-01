#!/usr/bin/python

import os, time

# Global path variables
dir_expr_root = "/tmp/"
dir_expr_folder = "/pcc_expr"
dir_expr_path = dir_expr_root + "/pcc_expr/"
dir_expr_bash = "/pcc_expr/experiments/bash/"
dir_expr_home = "/users/"

# The subdirectories of the PCC repo that should be copied
# to emulab.
copied_paths = [
"../experiments/*",
"../src/core/*",
"../src/pcc/*",
"../src/Makefile",
"../src/app/*",
"../python/models/*"
]

# Helper function definition

def get_hostname(node_id, username, expr, proj) :
  return username + "@" + node_id + "." + expr + "." + proj + ".emulab.net"

def remote_call(remote_host, cmd) :
  print ("ssh " + remote_host + " \"" + cmd + "\"")
  os.system("ssh -o StrictHostKeyChecking=no " + remote_host + " \"" + cmd + "\"")

def remote_call_background(remote_host, cmd) :
  print ("ssh " + remote_host + " \"" + cmd + "\" &")
  os.system("ssh -o StrictHostKeyChecking=no " + remote_host + " \"" + cmd + "\" &")

def remote_copy(path1, path2) :
  print ("scp -r " + path1 + " " + path2)
  os.system("scp -r " + path1 + " " + path2)

def prepare_install_dependencies(n_pair, username, expr, proj) :
  # should run on Ubuntu node
  sender_cmds = []
  receiver_cmds = []
  #cmds.append("sudo apt-get update")
  #cmds.append("sudo apt-get -y install python-dev python-pip vim iperf")
  #cmds.append("sudo -H pip install --upgrade pip")
  #cmds.append("sudo -H pip install --upgrade tensorflow")
  sender_cmds.append("sudo apt-get update")
  sender_cmds.append("sudo apt-get -y --force-yes install vim curl software-properties-common")
  sender_cmds.append("sudo add-apt-repository -y ppa:fkrull/deadsnakes")
  sender_cmds.append("sudo apt-get update")
  sender_cmds.append("sudo apt-get -y --force-yes install python3.5")
  sender_cmds.append("sudo apt-get -y --force-yes install python3.5-dev")
  sender_cmds.append("curl https://bootstrap.pypa.io/get-pip.py | sudo python3.5")
  sender_cmds.append("sudo apt-get -y install libopenmpi-dev")
  sender_cmds.append("sudo python3.5 -m pip install mpi4py")
  sender_cmds.append("sudo python3.5 -m pip install numpy")
  sender_cmds.append("sudo apt-get -y install libblas-dev liblapack-dev libatlas-base-dev gfortran")
  sender_cmds.append("sudo python3.5 -m pip install --upgrade pip")
  sender_cmds.append("sudo python3.5 -m pip install scipy")
  sender_cmds.append("sudo python3.5 -m pip install gym")
  sender_cmds.append("sudo python3.5 -m pip install tensorflow")
  sender_cmds.append("sudo apt -y install openmpi-bin")
  
  receiver_cmds.append("sudo apt-get update")
  receiver_cmds.append("sudo apt-get -y install vim software-properties-common")
  receiver_cmds.append("sudo add-apt-repository -y ppa:fkrull/deadsnakes")
  receiver_cmds.append("sudo apt-get update")
  receiver_cmds.append("sudo apt-get -y --force-yes install python3.5")
  receiver_cmds.append("sudo apt-get -y --force-yes install python3.5-dev")
  for i in range(1, n_pair + 1) :
    remote_sender = get_hostname("sender" + str(i), username, expr, proj)
    remote_receiver = get_hostname("receiver" + str(i), username, expr, proj)
    for cmd in sender_cmds:
      remote_call(remote_sender, cmd)

    for cmd in receiver_cmds:
      remote_call(remote_receiver, cmd)

def prepare_other_setup(n_pair, username, expr, proj) :
  remote_path = dir_expr_root + dir_expr_bash
  for i in range(1, n_pair + 1) :
    remote_sender = get_hostname("sender" + str(i), username, expr, proj)
    remote_receiver = get_hostname("receiver" + str(i), username, expr, proj)
    remote_call(remote_sender, remote_path + "/run_prepare.sh")
    remote_call(remote_receiver, remote_path + "/run_prepare.sh")

def prepare_file_copy(n_pair, username, expr, proj) :
  cur_path = os.path.dirname(os.path.realpath(__file__))
  for copied_path in copied_paths:
      copy_path = cur_path + "/" + copied_path
      remote_path = dir_expr_root + dir_expr_folder + copied_path[2:copied_path.rfind("/")]
      remote_host = get_hostname("bridge0", username, expr, proj)
      remote_call(remote_host, "mkdir -p " + remote_path)
      remote_copy(copy_path, remote_host + ":" + remote_path)
      for i in range(1, n_pair + 1):
          remote_host = get_hostname("sender" + str(i), username, expr, proj)
          remote_call(remote_host, "mkdir -p " + remote_path)
          remote_copy(copy_path, remote_host + ":" + remote_path)
          remote_host = get_hostname("receiver" + str(i), username, expr, proj)
          remote_call(remote_host, "mkdir -p " + remote_path)
          remote_copy(copy_path, remote_host + ":" + remote_path)


# Experiment classes

class PccExperiment:

  def __init__(self, args):
    # TODO: check args validity, e.g., bottleneck parameters should be positive
    print "Using %s@%s.%s.emulab.net" % (args.u, args.e, args.p)
    print "Experiment with %d node pair(s), repeated for %d times" % \
          (args.n, args.r)
    print "Per-node concurrency is %d" % args.c
    print "Inter-connection time gap is %d second(s)" % args.g
    print "Bridge pipe: "
    if args.interval :
      print "  bw    [%.2f, %.2f]Mbit/s" % (args.l_bandwidth, args.r_bandwidth)
      print "  delay [%.2f, %.2f]ms" % (args.l_delay, args.r_delay)
      print "  queue [%.2f, %.2f]KB" % (args.l_queue_size, args.r_queue_size)
      print "  plr   [%.2f, %.2f]" % (args.l_loss_rate, args.r_loss_rate)
      print "pipe changes every %d second(s), using random seed %d" % \
          (args.interval, args.random_seed)
    else :
      print "  bw %.2fMbit/s delay %.2fms queue %.2f KB plr %.2f" % \
          (args.l_bandwidth, args.l_delay, args.l_queue_size, args.l_loss_rate)

    self.emulab_user = args.u
    self.emulab_expr = args.e
    self.emulab_project = args.p
    self.expr_node_pair = args.n
    self.expr_concurrency = args.c
    self.expr_replica = args.r
    self.expr_duration = args.t
    self.expr_connection_gap = args.g

    self.skip_install = args.skip_install
    self.skip_copy = args.skip_copy
    self.skip_build = args.skip_build

    self.bridge_lbw = args.l_bandwidth
    self.bridge_rbw = args.r_bandwidth
    self.bridge_ldl = args.l_delay
    self.bridge_rdl = args.r_delay
    self.bridge_lqs = args.l_queue_size
    self.bridge_rqs = args.r_queue_size
    self.bridge_llr = args.l_loss_rate
    self.bridge_rlr = args.r_loss_rate
    self.bridge_intvl = args.interval
    self.seed = args.random_seed
    self.sender_args = args.args

  def prepare_build_pcc(self) :
    # only build PCC code once on node sender1, and then copy to other nodes
    remote_path = dir_expr_root + dir_expr_folder
    lib_path = dir_expr_path + "/src/core"
    lib_path_env = "setenv LD_LIBRARY_PATH \\\"" + lib_path + "\\\""
    cmds = []
    cmds.append("cd " + remote_path + "/src" + " && make clean && make")
    cmds.append("cp -r " + remote_path + " " + dir_expr_root)
    cmds.append("grep -q -F \'" + lib_path_env + "\' ~/.cshrc || echo \'" +
                    lib_path_env + "\' >> ~/.cshrc")
    for i in range(1, self.expr_node_pair + 1) :
      remote_sender = get_hostname("sender" + str(i), self.emulab_user,
                                   self.emulab_expr, self.emulab_project)
      remote_receiver = get_hostname("receiver" + str(i), self.emulab_user,
                                     self.emulab_expr, self.emulab_project)
      for cmd in cmds :
        remote_call(remote_sender, cmd)
        remote_call(remote_receiver, cmd)

  def clean_obsolete(self) :
    remote_call(get_hostname("bridge0", self.emulab_user, self.emulab_expr,
                             self.emulab_project),
                "sudo killall bash")
    for i in range(1, self.expr_node_pair + 1) :
      remote_call(get_hostname("sender" + str(i), self.emulab_user,
                               self.emulab_expr, self.emulab_project),
                  "sudo killall pccclient")
      remote_call(get_hostname("receiver" + str(i), self.emulab_user,
                               self.emulab_expr, self.emulab_project),
                  "sudo killall pccserver")

  def run_receiver(self, remote_host) :
      executable = dir_expr_path + "/src/app/pccserver"
      remote_call(remote_host,
                  "cd " + dir_expr_root + dir_expr_bash +
                      " && chmod a+x ./run_receiver.sh")
      remote_call(remote_host,
                  dir_expr_root + dir_expr_bash + "/run_receiver.sh " +
                      executable)

  def run_sender(self, remote_host, receiver_ip, duration, flow_id, timeshift) :
    # FIXME: timestamp here is local time, not emulab node time
    time_ms = int(round(time.time() * 1000))
    executable = dir_expr_path + "/src/app/pccclient"
    remote_call(remote_host,
                  "cd " + dir_expr_root + dir_expr_bash +
                      " && chmod a+x ./run_sender.sh")
    remote_call_background(remote_host,
                dir_expr_root + dir_expr_bash + "/run_sender.sh " + executable +
                " " + receiver_ip + " " + dir_expr_path + " " +
                str(self.expr_node_pair) + " " + str(self.bridge_lbw) + " " +
                str(self.bridge_ldl) + " " + str(self.bridge_lqs) + " " +
                str(self.bridge_llr) + " /dev/null /dev/null " +
                str(duration) + 
                " " + str(self.sender_args) + " -flowid=" + str(flow_id) + " -timeshift=" +
                str(timeshift) + " &")

  def prepare(self) :
    if not self.skip_install :
      prepare_install_dependencies(self.expr_node_pair, self.emulab_user,
                                   self.emulab_expr, self.emulab_project)
      prepare_other_setup(self.expr_node_pair, self.emulab_user,
                          self.emulab_expr, self.emulab_project)
    if not self.skip_copy :
      prepare_file_copy(self.expr_node_pair, self.emulab_user, self.emulab_expr,
                        self.emulab_project)
    if not self.skip_build :
      self.prepare_build_pcc()

  def run(self) :
    print "Kill old processes, if there's any still running"
    self.clean_obsolete()

    print "Start experiment now ..."
    for itr in range(0, self.expr_replica) :
      print "[repitition %d]" % itr
      print "Start receiver nodes"
      for i in range(1, self.expr_node_pair + 1) :
        self.run_receiver(get_hostname("receiver" + str(i), self.emulab_user,
                                       self.emulab_expr, self.emulab_project))
      time.sleep(2)

      print "Setup bridge node"
      remote_call_background(get_hostname("bridge0", self.emulab_user, self.emulab_expr,
                               self.emulab_project),
                  dir_expr_root + dir_expr_bash +
                      "/run_bridge_setup.sh " + str(self.expr_node_pair) +
                      " " + str(self.bridge_intvl) + " " + str(self.seed) +
                      " " + str(self.bridge_lbw) + " " + str(self.bridge_rbw) +
                      " " + str(self.bridge_ldl) + " " + str(self.bridge_rdl) +
                      " " + str(self.bridge_lqs) + " " + str(self.bridge_rqs) +
                      " " + str(self.bridge_llr) + " " + str(self.bridge_rlr) +
                      " " + self.emulab_user + " &")
      # sleep for a short period to account for the ssh delay
      time.sleep(1)

      start_time = time.time()
      # Each sender node initiates self.expr_concurrency connections
      print "Start sender nodes (per-node concurrency = %d) ..." % \
          self.expr_concurrency
      for concur in range(0, self.expr_concurrency) :
        for i in range(1, self.expr_node_pair + 1) :
          if i > 1 or concur > 0 :
            time.sleep(self.expr_connection_gap)
          receiver_ip = "10.1.1." + str(i + 2)
          print "-> sender %d in round %d" % (i, concur)
          self.run_sender(get_hostname("sender" + str(i), self.emulab_user,
                                       self.emulab_expr, self.emulab_project),
                          receiver_ip, self.expr_duration, i, time.time() - start_time)
      time.sleep(self.expr_duration - (time.time() - start_time))

      self.clean_obsolete()
      print "Copy bridge setup changing traces"
      remote_call(get_hostname("sender1", self.emulab_user, self.emulab_expr,
                               self.emulab_project),
                  "mv /users/" + self.emulab_user + "/pcc_log_bridge_* " +
                      dir_expr_path)
      print "[repitition %d finished]" % itr

    print "Experiment Finished!"

  def finish(self) :
    pass
    """
    os.system("mkdir -p results")
    for i in range(1, self.expr_node_pair + 1) :
      remote_copy(get_hostname("sender" + str(i), self.emulab_user,
                               self.emulab_expr, self.emulab_project) + ":" +
                      dir_expr_path + "/pcc_log_*",
                  "./results/")
    """

