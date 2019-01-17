import os

cfg = {}

cfg["REPO_DIR"] = "/home/pcc/PCC/deep-learning/"

cfg["EXPR_DIR"] = cfg["REPO_DIR"] + "experiments/"
cfg["PYTHON_ML_DIR"] = cfg["REPO_DIR"] + "python/rate_controllers/reinforcement_learning/"

cfg["ML_MODEL_PATH"] = cfg["PYTHON_ML_DIR"]
#### Experimenting with random loss
#cfg["ML_MODEL_PATH"] = cfg["REPO_DIR"] + "python/rate_controllers/reinforcement_learning_rand_loss/"
#### For loss-based
#cfg["ML_MODEL_PATH"] = cfg["REPO_DIR"] + "python/rate_controllers/rl_loss_based_2/"

cfg["UDT_SRC_DIR"] = cfg["REPO_DIR"] + "/src/"
cfg["UDT_LIB_DIR"] = cfg["UDT_SRC_DIR"] + "/core/"
cfg["SIM_DIR"] = cfg["REPO_DIR"] + "/sim/"

os.environ["LD_LIBRARY_PATH"] = cfg["UDT_LIB_DIR"]
