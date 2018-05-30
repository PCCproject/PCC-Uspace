#!/usr/bin/python

import argparse, sys, time, os, random
from experiments import PccExperiment

# Global path variables
dir_expr_root = "/tmp/"
dir_expr_folder = "/pcc_expr"
dir_expr_path = dir_expr_root + "/pcc_expr/"
dir_expr_bash = "/pcc_expr/experiments/bash/"
dir_expr_home = "/users/"

# The subdirectories of the PCC repo that should be copied
# to emulab.
copied_paths = [
"experiments/",
"src/",
"python/models/"
]

# Configuration variables
bottleneck_lbw = 0
bottleneck_rbw = 0
bottleneck_ldl = 0
bottleneck_rdl = 0
bottleneck_lbf = 0
bottleneck_rbf = 0
bottleneck_llr = 0
bottleneck_rlr = 0
flow_start_time = []
flow_end_time = []
flow_args = []
flow_proto = []
duration = 0

################################################################################
def generate_bridge_setup_script(args) :
  bottleneck_config = open(args.bottleneck_config, "r")
  # read bridge bottleneck setup configuration from file
  while True:
    s = bottleneck_config.readline()
    if s == "" :
      break
    if s == "\n" or s[0] == "#" :
      continue
    s = s.strip('\n').split(" ")
    if s[0] == "bw" :
      # with granularity of 1Mbps
      bottleneck_lbw = int(s[1])
      bottleneck_rbw = int(s[2])
    elif s[0] == "dl" :
      # with granularity of 1ms
      bottleneck_ldl = int(s[1])
      bottleneck_rdl = int(s[2])
    elif s[0] == "queue" :
      # with granularity of 1KB
      bottleneck_lbf = int(s[1])
      bottleneck_rbf = int(s[2])
    elif s[0] == "plr" :
      # with granularity of %0.1 / 0.001
      bottleneck_llr = float(s[1])
      bottleneck_rlr = float(s[2])
    elif s[0] == "interval" :
      interval = int(s[1])
  bottleneck_config.close()

  bottleneck_script = open("bash/run_bridge_setup.sh", "w")
  bottleneck_script.write("#!/usr/local/bin/bash\n\n")
  bottleneck_script.write("sudo ipfw delete 100\n")
  bottleneck_script.write("sudo ipfw pipe 100 config bw %dMbit/s delay %dms" %
                          (bottleneck_lbw, bottleneck_ldl))
  bottleneck_script.write(" queue %dKB plr %.3f\n\n" %
                          (bottleneck_lbf, bottleneck_llr))
  for i in range(1, args.n + 1) :
    bottleneck_script.write("sudo ipfw add 100 pipe 100 all from any to ")
    bottleneck_script.write("10.1.1.%d\n" % (i + 2))
  bottleneck_script.write("\n")
  if bottleneck_lbw == bottleneck_rbw and bottleneck_ldl == bottleneck_rdl and \
      bottleneck_lbf == bottleneck_rbf and bottleneck_llr == bottleneck_rlr :
    print "Static bridge setup:"
    print "  bw %.2fMbit/s delay %.2fms queue %.2f KB plr %.2f" % \
          (bottleneck_lbw, bottleneck_ldl, bottleneck_lbf, bottleneck_llr)
  else:
    print "Dynamic bridge setup:"
    print "  bw    [%.2f, %.2f]Mbit/s" % (bottleneck_lbw, bottleneck_rbw)
    print "  delay [%.2f, %.2f]ms" % (bottleneck_ldl, bottleneck_rdl)
    print "  queue [%.2f, %.2f]KB" % (bottleneck_lbf, bottleneck_rbf)
    print "  plr   [%.2f, %.2f]" % (bottleneck_llr, bottleneck_rlr)
    print "pipe changes every %d second(s)" % interval
    time_sec = 0
    while time_sec < duration :
      bw = random.randint(bottleneck_lbw, bottleneck_rbw)
      dl = random.randint(bottleneck_ldl, bottleneck_rdl)
      bf = random.randint(bottleneck_lbf, bottleneck_rbf)
      lr = random.uniform(bottleneck_llr, bottleneck_rlr)
      bottleneck_script.write("sudo ipfw pipe 100 config bw %dMbit/s " % bw)
      bottleneck_script.write("delay %dms queue %dKB plr %.3f\n" % (dl, bf, lr))
      bottleneck_script.write("sleep %d\n\n" % interval)
      time_sec += 5
  bottleneck_script.close()

  # # save a copy of the bridge setup script
  # time_ms = int(round(time.time() * 1000))
  # os.system("cp bash/run_bridge_setup.sh bash/run_bridge_setup_%d" % time_ms)
################################################################################


################################################################################
def generate_flow_configuration(args) :
  flow_config = open(args.flow_config, "r")
  flow_bbr = 0
  flow_cubic = 0
  global duration
  while True:
    s = flow_config.readline()
    if s == "" :
      break
    s = s.strip('\n').split(" ")
    flow_start_time.append(int(s[0]))
    flow_end_time.append(int(s[1]))
    flow_proto.append(s[2])
    duration = max(duration, int(s[1]))
    if s[2] == "PCC" :
      if len(s) == 3 :
        flow_args.append(args.args)
      else :
        flow_args.append(' '.join(s[3:]))
    elif s[2] == "bbr" :
      flow_bbr += 1
      flow_args.append("")
    elif s[2] == "cubic" :
      flow_cubic += 1
      flow_args.append("")
################################################################################


################################################################################
def process_config(args) :
  # FIXME: make sure the last '\n' line does not mass up
  # process flow configuration first to get the total experiment duration
  generate_flow_configuration(args)
  generate_bridge_setup_script(args)
################################################################################


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


################################################################################
def prepare_file_copy(args) :
  cur_path = os.path.dirname(os.path.realpath(__file__))
  #remote_path = dir_expr_home + args.u + dir_expr_folder + "/"
  remote_path = dir_expr_path 
  remote_host = get_hostname("sender1", args.u, args.e, args.p)
  if not args.skip_copy :
    for copied_path in copied_paths:
      copy_path = cur_path + "/../" + copied_path + "/*"
      remote_call(remote_host, "mkdir -p " + remote_path + copied_path)
      remote_copy(copy_path, remote_host + ":" + remote_path + copied_path)
  # has to copy the most recent bridge setup script every time
  remote_copy(cur_path + "/bash/run_bridge_setup.sh",
              remote_host + ":" + remote_path + "/experiments/bash/")
################################################################################


################################################################################
def prepare_install_dependencies(args) :
  # should run on Ubuntu node
  sender_cmds = []
  receiver_cmds = []

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
  sender_cmds.append("sudo python3.5 -m pip install tensorflow==1.5")
  sender_cmds.append("sudo apt -y install openmpi-bin")
  sender_cmds.append("sudo apt -y install iperf")

  receiver_cmds.append("sudo apt-get update")
  receiver_cmds.append("sudo apt-get -y install vim software-properties-common")
  receiver_cmds.append("sudo add-apt-repository -y ppa:fkrull/deadsnakes")
  receiver_cmds.append("sudo apt-get update")
  receiver_cmds.append("sudo apt-get -y --force-yes install python3.5")
  receiver_cmds.append("sudo apt-get -y --force-yes install python3.5-dev")
  receiver_cmds.append("sudo apt -y install iperf")
  for i in range(1, args.n + 1) :
    remote_sender = get_hostname("sender" + str(i), args.u, args.e, args.p)
    remote_receiver = get_hostname("receiver" + str(i), args.u, args.e, args.p)
    for cmd in sender_cmds:
      remote_call(remote_sender, cmd)
    for cmd in receiver_cmds:
      remote_call(remote_receiver, cmd)
################################################################################


################################################################################
def prepare_other_setup(args) :
  #remote_path = dir_expr_home + dir_expr_bash
  remote_path = dir_expr_path + dir_expr_bash
  for i in range(1, args.n + 1) :
    remote_sender = get_hostname("sender" + str(i), args.u, args.e, args.p)
    remote_receiver = get_hostname("receiver" + str(i), args.u, args.e, args.p)
    remote_call(remote_sender, remote_path + "/run_prepare.sh")
    remote_call(remote_receiver, remote_path + "/run_prepare.sh")
################################################################################


################################################################################
def prepare_build_pcc(args) :
    # only build PCC code once on node sender1, and then copy to other nodes
    #remote_path = dir_expr_home + args.u + dir_expr_folder
    remote_path = dir_expr_path
    #if not args.skip_build :
    #  remote_call(get_hostname("sender1", args.u, args.e, args.p),
    #              "cd " + remote_path + "/src" + " && make clean && make")
    lib_path = dir_expr_path + "/src/core"
    lib_path_env = "setenv LD_LIBRARY_PATH \\\"" + lib_path + "\\\""
    cmds = []
    if not args.skip_build :
      cmds.append("cd " + remote_path + "/src" + " && make clean && make")
    cmds.append("sudo chown -R " + args.u + ":" + args.p + " " + dir_expr_root)
    #cmds.append("cp -r " + remote_path + " " + dir_expr_root)
    cmds.append("grep -q -F \'" + lib_path_env + "\' ~/.cshrc || echo \'" +
                    lib_path_env + "\' >> ~/.cshrc")
    for i in range(1, args.n + 1) :
      remote_sender = get_hostname("sender" + str(i), args.u, args.e, args.p)
      remote_receiver = get_hostname("receiver" + str(i), args.u, args.e,
                                     args.p)
      for cmd in cmds :
        remote_call(remote_sender, cmd)
        remote_call(remote_receiver, cmd)

    remote_call(get_hostname("bridge0", args.u, args.e, args.p),
                "cp -r " + remote_path + " " + dir_expr_root)
################################################################################


################################################################################
def prepare(args) :
  prepare_file_copy(args)
  if not args.skip_install :
    prepare_install_dependencies(args)
    prepare_other_setup(args)
  prepare_build_pcc(args)
################################################################################


################################################################################
def clean_obsolete(args) :
    remote_call(get_hostname("bridge0", args.u, args.e, args.p),
                "sudo killall bash")
    for i in range(1, args.n + 1) :
      remote_call(get_hostname("sender" + str(i), args.u, args.e, args.p),
                  "sudo killall pccclient")
      remote_call(get_hostname("receiver" + str(i), args.u, args.e, args.p),
                  "sudo killall pccserver")
      remote_call(get_hostname("sender" + str(i), args.u, args.e, args.p),
                  "sudo killall iperf")
      remote_call(get_hostname("receiver" + str(i), args.u, args.e, args.p),
                  "sudo killall iperf")
################################################################################


################################################################################
def run_receivers(args) :
  print "Start PCC and Iperf on all receivers"
  executable = dir_expr_path + "/src/app/pccserver"
  for i in range(1, args.n + 1) :
    remote_host = get_hostname("receiver" + str(i), args.u, args.e, args.p)
    remote_call(remote_host,
                "cd " + dir_expr_root + dir_expr_bash +
                    " && chmod a+x ./run_pcc_receiver.sh")
    remote_call(remote_host,
                dir_expr_root + dir_expr_bash + "/run_pcc_receiver.sh " +
                    executable)
    remote_call_background(remote_host, "iperf -s &")
    time.sleep(1)
################################################################################


################################################################################
def run_bridge(args) :
  remote_host = get_hostname("bridge0", args.u, args.e, args.p)
  remote_call(remote_host,
              "cd " + dir_expr_root + dir_expr_bash +
                  " && chmod a+x ./run_bridge_setup.sh")
  remote_call_background(remote_host,
                         dir_expr_root + dir_expr_bash + "/run_bridge_setup.sh")
################################################################################


################################################################################
def run_sender_pcc(args,
                   flow_args,
                   remote_host,
                   receiver_ip,
                   flow_id,
                   duration,
                   timeshift) :
  # FIXME: timestamp here is local time, not emulab node time
  executable = dir_expr_path + "/src/app/pccclient"

  remote_call(remote_host,
                "cd " + dir_expr_root + dir_expr_bash +
                    " && chmod a+x ./run_pcc_sender.sh")
  # FIXME: bottleneck related arguments for dynamic setups
  remote_call_background(
      remote_host,
      dir_expr_root + dir_expr_bash + "/run_pcc_sender.sh " + executable + " " +
          receiver_ip + " " + dir_expr_path + " " + str(args.n) + " " +
          str(bottleneck_lbw) + " " + str(bottleneck_ldl) + " " +
          str(bottleneck_lbf) + " " + str(bottleneck_llr) +
          " /dev/null /dev/null " + str(duration) + " " + str(flow_args) +
          " -flowid=" + str(flow_id) + " -timeshift=" + str(timeshift) + " &")
################################################################################


################################################################################
def run_sender_tcp(args,
                   proto,
                   remote_host,
                   receiver_ip,
                   flow_id,
                   duration,
                   timeshift) :
  # FIXME: change log file names based on the PCC naming standard
  # FIXME: make sure BBR and CUBIC can be used from the same machine with -Z
  remote_call_background(
      remote_host,
      "cd " + dir_expr_path + " && iperf -c " + receiver_ip + " -i 1 -t " +
          str(duration) + " -Z " + proto + " >" + proto + "_log_flow" +
          str(flow_id) + "_" + str(bottleneck_lbw) + "_" + str(bottleneck_ldl) +
          "_" + str(bottleneck_lbf) + "_" + str(bottleneck_llr) + "_" +
          str(timeshift) + ".txt")
################################################################################


################################################################################
def run_senders(args) :
  # start running flows
  initial_start_time = flow_start_time[0]
  last_start_time = flow_start_time[0]
  end_time = flow_end_time[0]
  sender_indx = 0
  for i in range(0, len(flow_proto)) :
    time.sleep(flow_start_time[i] - last_start_time)
    last_start_time = flow_start_time[i]
    end_time = max(end_time, flow_end_time[i])
    timeshift = last_start_time - initial_start_time

    duration = flow_end_time[i] - flow_start_time[i]
    remote_host = get_hostname("sender" + str(sender_indx + 1), args.u, args.e,
                               args.p)
    receiver_ip = "10.1.1." + str(sender_indx + 3)
    sender_indx = (sender_indx + 1) % args.n

    if flow_proto[i] == "PCC" :
      run_sender_pcc(args, flow_args[i], remote_host, receiver_ip, i, duration,
                     timeshift)
    elif flow_proto[i] == "bbr" :
      run_sender_tcp(args, "bbr", remote_host, receiver_ip, i, duration,
                     timeshift)
    elif flow_proto[i] == "cubic" :
      run_sender_tcp(args, "cubic", remote_host, receiver_ip, i, duration,
                     timeshift)

  time.sleep(end_time - last_start_time)
################################################################################


################################################################################
def copy_logs(args) :
  os.system("mkdir -p results")
  for i in range(1, args.n + 1) :
    remote_copy(get_hostname("sender" + str(i), args.u, args.e, args.p) + ":" +
                    dir_expr_path + "/*_log_*",
                "./results/")
################################################################################




################################################################################
if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("-u", help="emulab username", required=True)
  parser.add_argument("-e", help="emulab experiment name", required=True)
  parser.add_argument("-p", help="emulab project", default="UIUCScheduling")
  parser.add_argument("-n", help="sender/receiver pair count", type=int,
                      default=1)
  parser.add_argument("-r", help="number of experiment repetitions", type=int,
                      default=2)

  parser.add_argument("--flow-config", "-fc", help="flow configuration file \
                      path", required=True)
  parser.add_argument("--bottleneck-config", "-bc", help="bottleneck \
                      configuration file path", required=True)

  parser.add_argument("--skip-install", "-ski", help="Skip dependency install",
                      action="store_true")
  parser.add_argument("--skip-copy", "-skc", help="Skip remote file copy",
                      action="store_true")
  parser.add_argument("--skip-build", "-skb", help="Skip PCC code make",
                      action="store_true")

  # add default PCC arguments here
  parser.add_argument("--args", "-a", help="PCC sender arguments",
                      type=str, default="")
  args = parser.parse_args()

  if not os.path.isfile(args.bottleneck_config) :
    print "Invalid bottleneck configuration file path"
    sys.exit()
  if not os.path.isfile(args.flow_config) :
    print "Invalid flow configuration file path"
    sys.exit()

  process_config(args)
  prepare(args)

  for itr in range(0, args.r) :
    print "[repitition %d]" % itr
    clean_obsolete(args)
    run_receivers(args)
    run_bridge(args)
    time.sleep(1)
    run_senders(args)

  print "Experiment finished"
  time.sleep(1)
  clean_obsolete(args)
  copy_logs(args)

