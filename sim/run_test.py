import os
import sys

img_name = "graph.png"
img_dir = "./test_imgs/"
test_bw  = 70.0
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

    if "--test-bw=" in arg:
        test_bw = float(arg_val)

    if "--test-reset-bw=" in arg:
        reset_bw = float(arg_val)

    if "--test-img-name=" in arg:
        img_name = arg_val

    if "--test-img-dir=" in arg:
        img_dir = arg_val

if ("--ml"in sys.argv):

    log = "pcc_log.txt"
    log_dir = "/home/njay2/PCC/restructure/sim/test_logs/"
    pyhelper = "pcc_gym_driver"
    pypath = "/home/njay2/PCC/deep-learning/python/models/gym-expr/"
    gamma = "0.98"
    train_model_name = "/home/njay2/PCC/restructure/sim/cur_model"
    model_name = "/home/njay2/PCC/restructure/sim/test_models/cur_model"

    if (should_cp):
        cmd = "cp " + train_model_name + "* " + model_name[:model_name.rfind("/")]
        print(cmd)
        os.system(cmd)

    cmd = "./sim_test --sim-dur=30 --test-bw=" + str(test_bw) + " -log=" + log_dir + log + " --reset-target-rate=" + str(reset_bw) + " -pyhelper=" + pyhelper + " -pypath=" + pypath + " --deterministic --inverted-exponent-utility --gamma=" + gamma + " --model-name=" + model_name + " --load-model --no-training --log-utility-calc-lite --python-rate-control --no-reset"
    #cmd = "./sim_test --sim-dur=30 --test-bw=" + str(test_bw) + " -log=" + log_dir + log + " --reset-target-rate=" + str(reset_bw) + " -pyhelper=" + pyhelper + " -pypath=" + pypath + " --inverted-exponent-utility --gamma=" + gamma + " --model-name=" + model_name + " --load-model --no-training --log-utility-calc-lite --python-rate-control --no-reset"
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
