#!/usr/bin/python

import argparse
from experiments import PccStaticExperiment

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

  # Emulab bridge node setups
  parser.add_argument("--bandwidth", "-b", help="Bottleneck bandwidth (Mbps)",
                      type=float, default=100)
  parser.add_argument("--delay", "-d", help="Bottleneck RTT (ms)", type=float,
                      default=30)
  parser.add_argument("--queue-size", "-q", help="Bottleneck buffer (KB)",
                      type=float, default=75)
  parser.add_argument("--loss-rate", "-l", help="Bottleneck random loss rate",
                      type=float, default=0)
  #TODO: necessary for changing network conditions
  #parser.add_argument("--bridge-script", "-bs", help="Bridge node setup script",
  #                    default="")
  args = parser.parse_args()

  expr = PccStaticExperiment(args)
  #expr.prepare()
  expr.run()
  expr.finish()
