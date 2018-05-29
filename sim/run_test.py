import os
import sys

img_name = "graph.png"
img_dir = "./test_imgs/"
reset_bw = 20.0
show = False

if ("--show" in sys.argv):
    show = True

should_cp = True
if ("--no-cp" in sys.argv):
    should_cp = False

for arg in sys.argv:
    arg_val = arg
    if ("=" in arg):
        arg_val = arg[arg.find("=") + 1:]

    if "--test-reset-bw=" in arg:
        reset_bw = float(arg_val)

    if "--test-img-name=" in arg:
        img_name = arg_val

    if "--test-img-dir=" in arg:
        img_dir = arg_val

if ("--ml"in sys.argv):

    log = "pcc_log.txt"
    log_dir = "/home/njay2/PCC/restructure/sim/test_logs/"
    pyhelper = "training_client"
    pypath = "/home/njay2/PCC/deep-learning/python/models/gym-rpc/"
    gamma = "0.98"
    train_model_name = "/home/njay2/PCC/deep-learning/python/models/gym-rpc/cur_model"
    model_path = "/home/njay2/PCC/restructure/sim/test_models/"
    model_name = "cur_model"

    if (should_cp):
        cmd = "cp " + train_model_name + "* " + model_path
        print(cmd)
        os.system(cmd)

    cmd = "./sim_test"
    cmd += " --sim-dur=30"
    cmd += " -log=" + log_dir + log
    cmd += " --reset-target-rate=" + str(reset_bw)
    cmd += " --pcc-utility-calc=linear"
    cmd += " --gamma=" + gamma
    cmd += " --model-name=" + model_name
    cmd += " --model-path=" + model_path
    cmd += " --no-training"
    cmd += " --log-utility-calc-lite"
    cmd += " --pcc-rate-control=python"
    cmd += " -pyhelper=" + pyhelper
    cmd += " -pypath=" + pypath
    cmd += " --no-reset"
    cmd += " --deterministic"
    for arg in sys.argv:
        if "--test" in arg:
            cmd += " " + arg
    print(cmd)
    os.system(cmd)

    cmd = "sudo killall sim_test"
    print(cmd)
    os.system(cmd)

    grapher = "/home/njay2/PCC/deep-learning/vis/pcc_grapher.py"
    graph = "/home/njay2/PCC/restructure/vis/graphs/basic.json"

    cmd = "python " + grapher + " " + log_dir + " " + graph
    if (not show):
        cmd += " --output=" + img_dir + img_name
    print(cmd)
    os.system(cmd)

else:
    
    log = "pcc_log.txt"
    log_dir = "/home/njay2/PCC/restructure/sim/logs/"

    cmd = "./test -log=" + log_dir + log + " --inverted-exponent-utility --log-utility-calc-lite"

    print(cmd)
    os.system(cmd)

    grapher = "/home/njay2/PCC/deep-learning/vis/pcc_grapher.py"
    graph = "/home/njay2/PCC/restructure/vis/graphs/basic.json"

    os.system("python " + grapher + " " + log_dir + " " + graph)
