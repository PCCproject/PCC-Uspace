from gym.envs.registration import register
import src.pcc_addon

register(
    id='PCCLogEnv-v0',
    entry_point='impl.pcc_env.pcc_log_env:PccLogEnv',
    kwargs={'log_file' : 'logs/pcc_log_1516039168.txt', 'epsilon' : 4, 'k' : 2},
    max_episode_steps=100,
    reward_threshold=0.78, # optimum = .8196
)

register(
    id='PCCEnv-v0',
    entry_point='impl.pcc_env.pcc_rl_env:PccEnv',
    kwargs={'epsilon' : 4, 'k' : 2},
    max_episode_steps=100,
    reward_threshold=0.78, # optimum = .8196
)