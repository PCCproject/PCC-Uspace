#!/usr/bin/python

import os, time

# Global path variables
dir_expr_root = "/local/"
dir_expr_folder = "/pcc_expr"
dir_expr_path = dir_expr_root + "/pcc_expr/"
dir_expr_bash = "/pcc_expr/experiments/bash/"
dir_expr_home = "/users/"


# Helper function definition

def get_hostname(node_id, username, expr, proj) :
  return username + "@" + node_id + "." + expr + "." + proj + ".emulab.net"

def remote_call(remote_host, cmd) :
  print ("ssh " + remote_host + " \"" + cmd + "\"")
  os.system("ssh " + remote_host + " \"" + cmd + "\"")

def remote_copy(path1, path2) :
  print ("scp -r " + path1 + " " + path2)
  os.system("scp -r " + path1 + " " + path2)

def prepare_install_dependencies(n_pair, username, expr, proj) :
  # should run on Ubuntu node
  cmds = []
  cmds.append("sudo apt-get update")
  cmds.append("sudo apt-get -y install python-dev python-pip vim iperf")
  cmds.append("sudo -H pip install --upgrade pip")
  cmds.append("sudo -H pip install --upgrade tensorflow")
  for i in range(1, n_pair + 1) :
    remote_sender = get_hostname("sender" + str(i), username, expr, proj)
    remote_receiver = get_hostname("receiver" + str(i), username, expr, proj)
    for cmd in cmds :
      remote_call(remote_sender, cmd)
      remote_call(remote_receiver, cmd)

def prepare_other_setup(n_pair, username, expr, proj) :
  remote_path = dir_expr_home + username + dir_expr_bash
  for i in range(1, n_pair + 1) :
    remote_sender = get_hostname("sender" + str(i), username, expr, proj)
    remote_receiver = get_hostname("receiver" + str(i), username, expr, proj)
    remote_call(remote_sender, remote_path + "/run_prepare.sh")
    remote_call(remote_receiver, remote_path + "/run_prepare.sh")
  # Initialize bridge setup (remember to modify pipe before experiment)
  remote_call(get_hostname("bridge0", username, expr, proj),
              remote_path + "/run_bridge_setup.sh " + str(n_pair))


# Experiment classes

class PccStaticExperiment:

  def __init__(self, args):
    print "Using %s@%s.%s.emulab.net" % (args.u, args.e, args.p)
    print "Simple experiment with %d node pair(s), repeated for %d times" % \
          (args.n, args.r)
    print "Bridge pipe: bw %.2fMbit/s delay %.2fms queue %.2fKB plr %.3f" % \
          (args.bandwidth, args.delay, args.queue_size, args.loss_rate)

    self.emulab_user = args.u
    self.emulab_expr = args.e
    self.emulab_project = args.p
    self.expr_node_pair = args.n
    self.expr_concurrency = args.c
    self.expr_replica = args.r
    self.expr_duration = args.t
    self.bridge_bw = args.bandwidth
    self.bridge_dl = args.delay
    self.bridge_qs = args.queue_size
    self.bridge_lr = args.loss_rate
    #self.bridge_bs = args.bridge_script

    self.sender_args = "send 10.1.1.3 9000 -DEBUG_RATE_CONTROL -DEBUG_UTILITY_CALC -LOG_RATE_CONTROL_PARAMS"
    self.receiver_args = "recv 9000"

  def prepare_build_pcc(self) :
    # only build PCC code once on node sender1, and then copy to other nodes
    copy_path = os.path.dirname(os.path.realpath(__file__)) + "/../*"
    remote_path = dir_expr_home + self.emulab_user + dir_expr_folder
    remote_host = get_hostname("sender1", self.emulab_user, self.emulab_expr,
                               self.emulab_project)
    remote_call(remote_host, "mkdir -p " + remote_path)
    remote_copy(copy_path, remote_host + ":" + remote_path)
    remote_call(remote_host,
                "cd " + remote_path + "/src" + " && make clean && make")
    lib_path = dir_expr_path + "/src/core"
    lib_path_env = "setenv LD_LIBRARY_PATH \\\"" + lib_path + "\\\""
    cmds = []
    cmds.append("sudo chown -R " + self.emulab_user + ":" +
                    self.emulab_project + " " + dir_expr_root)
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

  def run_receiver(self, remote_host) :
      executable = dir_expr_path + "/src/app/pccserver"
      remote_call(remote_host,
                  "cd " + dir_expr_root + dir_expr_bash +
                      " && chmod a+x ./run_receiver.sh")
      remote_call(remote_host,
                  dir_expr_root + dir_expr_bash + "/run_receiver.sh " +
                      executable)

  def run_sender(self, remote_host, receiver_ip, duration) :
    # FIXME: timestamp here is local time, not emulab node time
    time_ms = int(round(time.time() * 1000))
    executable = dir_expr_path + "/src/app/pccclient"
    remote_call(remote_host,
                  "cd " + dir_expr_root + dir_expr_bash +
                      " && chmod a+x ./run_sender.sh")
    remote_call(remote_host,
                dir_expr_root + dir_expr_bash + "/run_sender.sh " + executable +
                " " + receiver_ip + " " + dir_expr_path + " " +
                str(self.expr_node_pair) + " " + str(self.bridge_bw) + " " +
                str(self.bridge_dl) + " " + str(self.bridge_qs) + " " +
                str(self.bridge_lr) + " /dev/null /dev/null " +
                str(duration) + " &")

  def prepare(self) :
    prepare_install_dependencies(self.expr_node_pair, self.emulab_user,
                                 self.emulab_expr, self.emulab_project)
    prepare_other_setup(self.expr_node_pair, self.emulab_user, self.emulab_expr,
                        self.emulab_project)
    self.prepare_build_pcc()

  def run(self) :
    print "Kill old processes, if there's any still running"
    for i in range(1, self.expr_node_pair + 1) :
      remote_call(get_hostname("sender" + str(i), self.emulab_user,
                               self.emulab_expr, self.emulab_project),
                  "sudo killall pccclient")
      remote_call(get_hostname("receiver" + str(i), self.emulab_user,
                               self.emulab_expr, self.emulab_project),
                  "sudo killall pccserver")

    print "Start experiment now ..."
    print "Setup bridge node"
    remote_call(get_hostname("bridge0", self.emulab_user, self.emulab_expr,
                             self.emulab_project),
                "sudo ipfw pipe 100 config bw " + str(self.bridge_bw) +
                    "Mbit/s delay " + str(self.bridge_dl) + "ms queue " +
                    str(self.bridge_qs) + "KB plr " + str(self.bridge_lr))
    time.sleep(1)

    # interval between starting multiple connections
    interval = 30
    for itr in range(0, self.expr_replica) :
      print "[repitition %d]" % itr
      print "Start receiver nodes"
      for i in range(1, self.expr_node_pair + 1) :
        self.run_receiver(get_hostname("receiver" + str(i), self.emulab_user,
                                       self.emulab_expr, self.emulab_project))
      time.sleep(2)
      # Each sender node initiates self.expr_concurrency connections
      print "Start sender nodes (per-node concurrency = %d) ..." % \
          self.expr_concurrency
      for concur in range(0, self.expr_concurrency) :
        for i in range(1, self.expr_node_pair + 1) :
          if i > 1 or concur > 0 :
            time.sleep(interval)
          receiver_ip = "10.1.1." + str(i + 2)
          print "-> sender %d in round %d" % (i, concur)
          self.run_sender(get_hostname("sender" + str(i), self.emulab_user,
                                       self.emulab_expr, self.emulab_project),
                          receiver_ip, self.expr_duration)
      time.sleep(self.expr_duration)

      print "Terminate receiver nodes"
      for i in range(1, self.expr_node_pair + 1) :
        remote_call(get_hostname("receiver" + str(i), self.emulab_user,
                                 self.emulab_expr, self.emulab_project),
                    "sudo killall pccserver")
      print "[repitition %d finished]" % itr

    print "Experiment Finished!"

  def finish(self) :
    os.system("mkdir -p results")
    for i in range(1, self.expr_node_pair + 1) :
      remote_copy(get_hostname("sender" + str(i), self.emulab_user,
                               self.emulab_expr, self.emulab_project) + ":" +
                      dir_expr_path + "/pcc_log_*",
                  "./results/")

