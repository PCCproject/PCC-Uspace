import gym
import network_sim
import tensorflow as tf

from stable_baselines.common.policies import MlpPolicy
from stable_baselines.common.policies import MlpLstmPolicy
from stable_baselines.common.policies import FeedForwardPolicy
from stable_baselines.common.vec_env import SubprocVecEnv
from stable_baselines import PPO1
from stable_baselines import TRPO

class MyMlpPolicy(FeedForwardPolicy):

    def __init__(self, sess, ob_space, ac_space, n_env, n_steps, n_batch, net_arch=[32, 16], reuse=False, **_kwargs):
        super(MyMlpPolicy, self).__init__(sess, ob_space, ac_space, n_env, n_steps, n_batch, reuse,
                                        feature_extraction="mlp", **_kwargs)

n_cpu = 1
env = gym.make('PccNs-v0')
#env = gym.make('CartPole-v0')

model = PPO1(MyMlpPolicy, env, verbose=1, schedule='constant', timesteps_per_actorbatch=8192, optim_batchsize=2048)
model.learn(total_timesteps=(4800 * 410))

##
#   Save the model to the location specified below.
##
export_dir = "/home/pcc/PCC/deep-learning/python/saved_models/mlp_sf_10hist_wide_queue/"
with model.graph.as_default():

    pol = model.policy_pi#act_model

    obs_ph = pol.obs_ph
    act = pol.deterministic_action
    sampled_act = pol.action

    obs_input = tf.saved_model.utils.build_tensor_info(obs_ph)
    outputs_tensor_info = tf.saved_model.utils.build_tensor_info(act)
    stochastic_act_tensor_info = tf.saved_model.utils.build_tensor_info(sampled_act)
    signature = tf.saved_model.signature_def_utils.build_signature_def(
        inputs={"ob":obs_input},
        outputs={"act":outputs_tensor_info, "stochastic_act":stochastic_act_tensor_info},
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
