import tensorflow as tf
import gym

import open_ai.common.tf_util as U
from open_ai.common.distributions import make_pdtype
from open_ai.common.tf_util import dense

##
#   A multi-layer perceptron model. Parameterized mostly by it's hidden layer size and number
#   of layers.
##
class MlpPolicy(object):
    recurrent = False

    ##
    #   A fairly simple placeholder initialization method. To run models that can be
    #   instantiated out of the training module, we need to be able to instantiate it without
    #   fully defining it. Added the _init method to satisfy this need.
    ##
    def __init__(self, name, reuse=False, *args, **kwargs):
        with tf.variable_scope(name):
            if reuse:
                tf.get_variable_scope().reuse_variables()
            self._init(*args, **kwargs)
            self.scope = tf.get_variable_scope().name

    ##
    #   A separate _init method allows us to instatiate the model outside the training algorithm
    #   and later give it the parameters to operate.
    ##
    def _init(self, ob_space, ac_space, hid_size, num_hid_layers, gaussian_fixed_var=True):
        assert isinstance(ob_space, gym.spaces.Box)

        self.pdtype = pdtype = make_pdtype(ac_space)
        sequence_length = None

        # Determine the observation space. I think the shape is basically just the observation
        # space for an unknown number of inputs
        ob = U.get_placeholder(name="ob", dtype=tf.float32,
            shape=[sequence_length] + list(ob_space.shape))

        # Constructing the value function. For this policy, the value function is similar to the
        # policy itself.
        last_out = ob
        for i in range(num_hid_layers):
            last_out = tf.nn.relu(dense(last_out, hid_size, "vffc%i" % (i+1), weight_init=U.normc_initializer(1.0)))
        self.vpred = dense(last_out, 1, "vffinal", weight_init=U.normc_initializer(1.0))[:, 0]

        # Constructing the policy function. Just consecutive layers of perceptrons, followed by
        # a probability density function.
        last_out = ob
        for i in range(num_hid_layers):
            last_out = tf.nn.relu(dense(last_out, hid_size, "polfc%i" % (i+1), weight_init=U.normc_initializer(1.0)))

        # If we are using a fixed-variance gaussian, then our output is chosen with using a
        # normal distribution with the variance fixed. Otherwise we are allowed to learn the
        # variance of our output distribution.
        if gaussian_fixed_var and isinstance(ac_space, gym.spaces.Box):
            mean = dense(last_out, pdtype.param_shape()[0]//2, "polfinal", U.normc_initializer(0.01))
            logstd = tf.get_variable(name="logstd", shape=[1, pdtype.param_shape()[0]//2], initializer=tf.zeros_initializer())
            pdparam = tf.concat([mean, mean * 0.0 + logstd], axis=1)
        else:
            pdparam = dense(last_out, pdtype.param_shape()[0], "polfinal", U.normc_initializer(0.01))

        self.pd = pdtype.pdfromflat(pdparam)

        self.state_in = []
        self.state_out = []

        # change for BC
        stochastic = U.get_placeholder(name="stochastic", dtype=tf.bool, shape=())
        ac = U.switch(stochastic, self.pd.sample(), self.pd.mode())
        self.ac = ac
        self._act = U.function([stochastic, ob], [ac, self.vpred])

    def act(self, stochastic, ob):
        ac1, vpred1 = self._act(stochastic, ob[None])
        return ac1[0], vpred1[0], 0, 0 #h_state, c_state

    def reset_state(self):
        pass

    def get_variables(self):
        return tf.get_collection(tf.GraphKeys.GLOBAL_VARIABLES, self.scope)

    def get_trainable_variables(self):
        return tf.get_collection(tf.GraphKeys.TRAINABLE_VARIABLES, self.scope)

    def get_initial_state(self):
        return []
