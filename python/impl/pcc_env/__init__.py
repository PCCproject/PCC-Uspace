from gym.envs.registration import register

register(
    id='PCCEnv-v0',
    entry_point='impl.pcc_env.pcc_env:PccEnv',
    kwargs={'log_file' : '<log address>'}, ## Need to feed in the log file here to be passed as an argument
    max_episode_steps=100,
    reward_threshold=0.78, # optimum = .8196
)