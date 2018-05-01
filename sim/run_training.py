import os
import sys

log = "pcc_log.txt"
log_dir = "/home/njay2/PCC/restructure/sim/logs/"
pyhelper = "pcc_gym_driver"
pypath = "/home/njay2/PCC/deep-learning/python/models/gym-expr/"
gamma = "0.98"
model_name = "/home/njay2/PCC/restructure/sim/cur_model"

cmd = "./test --sim-variable-link --sim-dur=1000000 -log=" + log_dir + log + " -pyhelper=" + pyhelper + " -pypath=" + pypath + " --inverted-exponent-utility --gamma=" + gamma + " --model-name=" + model_name + " --save-model --log-utility-calc-lite --python-rate-control"
#cmd = "./test --sim-dur=1000000 -log=" + log_dir + log + " -pyhelper=" + pyhelper + " -pypath=" + pypath + " --inverted-exponent-utility --gamma=" + gamma + " --model-name=" + model_name + " --save-model --log-utility-calc-lite --python-rate-control"

print(cmd)
os.system(cmd)
