#!/usr/bin/python

import argparse
from experiments import PccExperiment

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("-u", help="emulab username", required=True)
  parser.add_argument("-e", help="emulab experiment name", required=True)
  parser.add_argument("-p", help="emulab project", default="UIUCScheduling")
  parser.add_argument("-n", help="sender/receiver pair count", type=int,
                      default=1)
  parser.add_argument("-c", help="concurrency (connections per node pair)",
                      type=int, default=1)
  parser.add_argument("-r", help="number of experiment repetitions", type=int,
                      default=2)
  parser.add_argument("-t", help="duration per connection (sec)", type=int,
                      default=60)
  parser.add_argument("-g", help="Time gap between starting two connections",
                      type=int, default=15)

  parser.add_argument("--skip-install", "-ski", help="Skip dependency install",
                      action="store_true")
  parser.add_argument("--skip-copy", "-skc", help="Skip remote file copy",
                      action="store_true")
  parser.add_argument("--skip-build", "-skb", help="Skip PCC code make",
                      action="store_true")

  # Emulab bridge node setups
  parser.add_argument("--l-bandwidth", "-lb", help="Lower bound for uniform \
                      bottleneck bandwidth (Mbps)", type=int, default=100)
  parser.add_argument("--r-bandwidth", "-rb", help="Upper bound for uniform \
                      bottleneck bandwidth (Mbps)", type=int, default=100)
  parser.add_argument("--l-delay", "-ld", help="Lower bound for uniform \
                      bottleneck RTT (ms)", type=int, default=30)
  parser.add_argument("--r-delay", "-rd", help="Upper bound for uniform \
                      bottleneck RTT (ms)", type=int, default=30)
  parser.add_argument("--l-queue-size", "-lq", help="Lower bound for uniform \
                      bottleneck queue (KB)", type=int, default=75)
  parser.add_argument("--r-queue-size", "-rq", help="Upper bound for uniform \
                      bottleneck queue (KB)", type=int, default=75)
  parser.add_argument("--l-loss-rate", "-ll", help="Lower bound for uniform \
                      bottleneck random loss rate", type=float, default=0)
  parser.add_argument("--r-loss-rate", "-rl", help="Upper bound for uniform \
                      bottleneck random loss rate", type=float, default=0)
  parser.add_argument("--interval", "-i", help="Uniformly change bottleneck \
                      setup every INTERVAL (sec)", type=int, default=0)
  parser.add_argument("--random-seed", "-s", help="Random seed for bottleneck",
                      type=int, default=1)

  parser.add_argument("--args", "-a", help="PCC sender arguments",
                      type=str, default="")
  #TODO: necessary for changing network conditions
  #parser.add_argument("--bridge-script", "-bs", help="Bridge node setup script",
  #                    default="")
  args = parser.parse_args()

  expr = PccExperiment(args)
  expr.prepare()
  expr.run()
  expr.finish()
