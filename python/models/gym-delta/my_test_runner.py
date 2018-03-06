import os
import time

replicas = 3

params = {
"--delta-rate-scale=":[0.1, 0.15],
}
"""
"--history-len=":[1, 10000],
"--all-rate-scale=":[0.01, 100.0],
"--delta-rate-scale=":[0.01, 100.0],
"--utility-scale=":[1e-4, 1e-1],
"--rate-scale=":[1e-4, 1e-1],
"--latency-scale=":[1e-9, 1e-5],
"--loss-scale=":[1e-2, 1e2],
"--hid-layers=":[1, 7],
"--hid-size=":[8, 128],
"--ts-per-batch=":[32, 512],
"--max-kl=":[0.0001, 0.01],
"--cg-iters=":[1, 100],
"--cg-damping=":[1e-4, 1e-2],
"--gamma=":[0.1, 0.99],
"--lambda=":[0.1, 1],
"--vf-iters=":[1, 30], 
"--vf-stepsize=":[1e-5, 1e-3],
"--entcoeff=":[0.1, 1.0]
}
"""

def get_scaled_list(bounds):
    lower = bounds[0]
    upper = bounds[1]
    result = []
    is_int = isinstance(bounds[0], int)
    step = 1.6
    cur = float(lower)
    while cur <= upper:
        if (is_int):
            result.append(int(cur))
        else:
            result.append(cur)
        cur *= step
    return result

j = 0
for param in params.keys():
    print "Working on parameter " + str(j) + "/" + str(len(params.keys()))
    for i in range(0, replicas):
        for value in get_scaled_list(params[param]):
            os.system("python3.5 ./my_gym_test.py " + param + str(value) + " -log=test_log_" + str(time.time()) + ".txt &")
            time.sleep(500)
            os.system("killall python3.5")
print "Done!"
