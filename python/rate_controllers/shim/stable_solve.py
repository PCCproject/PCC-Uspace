import gym
import network_sim
import tensorflow as tf

from stable_baselines.common.policies import MlpPolicy
from stable_baselines.common.policies import MlpLstmPolicy
from stable_baselines.common.policies import LstmPolicy
from stable_baselines.common.vec_env import SubprocVecEnv
from stable_baselines import TRPO
from stable_baselines import PPO1
from stable_baselines import PPO2

class MyLstmPolicy(LstmPolicy):
   def __init__(self, sess, ob_space, ac_space, n_env, n_steps, n_batch, n_lstm=64, reuse=False, **_kwargs):
        super(MyLstmPolicy, self).__init__(sess, ob_space, ac_space, n_env, n_steps, n_batch, n_lstm, reuse,
                                            layer_norm=False, feature_extraction="mlp", **_kwargs) 

# multiprocess environment
n_cpu = 2
print("Creating environment.")
env = SubprocVecEnv([lambda: gym.make('PccNs-v0') for i in range(n_cpu)])
#env = SubprocVecEnv([lambda: gym.make('CartPole-v0') for i in range(n_cpu)])

print("Creating model.")
# Does NOT work -- TRPO and PPO are both broken.
#model = TRPO(MyLstmPolicy, env, verbose=1, timesteps_per_batch=2048)
model = PPO2(MlpPolicy, env, verbose=1, nminibatches=1, n_steps=1024, noptepochs=1)

print("Running training.")
model.learn(total_timesteps=1600 * 410)
model.save("ppo2_custom_env")

export_dir = "/home/pcc/PCC/deep-learning/python/saved_models/binary_lstm/"

with model.graph.as_default():

    pol = model.act_model
    #pol = model.policy_pi

    obs_ph = pol.obs_ph
    state_ph = pol.states_ph
    act = pol.deterministic_action
    state = pol.snew
    sampled_act = pol.action
    mask_ph = pol.masks_ph

    obs_input = tf.saved_model.utils.build_tensor_info(obs_ph)
    state_input = tf.saved_model.utils.build_tensor_info(state_ph)
    mask_input = tf.saved_model.utils.build_tensor_info(mask_ph)
    outputs_tensor_info = tf.saved_model.utils.build_tensor_info(act)
    state_tensor_info = tf.saved_model.utils.build_tensor_info(state)
    stochastic_act_tensor_info = tf.saved_model.utils.build_tensor_info(sampled_act)
    signature = tf.saved_model.signature_def_utils.build_signature_def(
        inputs={"ob":obs_input, "state":state_input, "mask": mask_input},
        outputs={"act":outputs_tensor_info, "stochastic_act":stochastic_act_tensor_info, 
"state":state_tensor_info},
        method_name=tf.saved_model.signature_constants.PREDICT_METHOD_NAME)

    #"""
    signature_map = {tf.saved_model.signature_constants.DEFAULT_SERVING_SIGNATURE_DEF_KEY:
                     signature}

    model_builder = tf.saved_model.builder.SavedModelBuilder(export_dir)
    model_builder.add_meta_graph_and_variables(model.sess,
        tags=[tf.saved_model.tag_constants.SERVING],
        signature_def_map=signature_map,
        clear_devices=True)
    model_builder.save(as_text=True)
