import os
import sys

def img_name(base_name, i):
    num_str = str(i)
    while (len(num_str) < 5):
        num_str = "0" + num_str
    return base_name + "_" + num_str + ".png"

def img_dir(start, bw):
    return "imgs_" + str(start) + "_" + str(bw) + "/"

bws = [30, 50, 70]
starts = [20, 100]

cmds = []
for bw in bws:
    for start in starts:
        os.system("mkdir " + img_dir(start, bw))
        cmds.append("python run_test.py --ml --test-bw=" + str(bw) + " --test-reset-bw=" + str(start) + " --test-img-dir=" + img_dir(start, bw))
        if (len(cmds) > 1):
            cmds[0] += " --no-cp"

for i in range(0, 1000):
    for cmd in cmds:
        os.system(cmd + " --test-img-name=" + str(img_name("graph", i)))
