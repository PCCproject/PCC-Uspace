import os
import sys

cmd = sys.argv[1]
for arg in sys.argv[2:]:
    cmd += " " + arg

while (True):
    print(cmd)
    os.system(cmd)

