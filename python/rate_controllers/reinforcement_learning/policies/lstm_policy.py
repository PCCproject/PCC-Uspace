import numpy as np                                                                               
import tensorflow as tf                                                                          
import gym
import open_ai.common.tf_util as U
from open_ai.common.tf_util import dense
from open_ai.common.distributions import make_pdtype

##
#   Create an LSTM policy. This object currently has several parameters baked-in to make it
#   easier to debug and understand input/output shape.
##
class LstmPolicy(object):

    def __init__(self, name, reuse=False, *args, **kwargs):
        with tf.variable_scope(name):
            if reuse:
                tf.get_variable_scope().reuse_variables()
            self._init(*args, **kwargs)
            self.scope = tf.get_variable_scope().name

    def _init(self, ob_space, ac_space, reuse=False, gaussian_fixed_var=False):
        
        # Re-create our action space as a probabilistic space, because we want to sample from it
        # with each of our actions.
        self.pdtype = pdtype = make_pdtype(ac_space)

        self.batch_size = tf.placeholder(dtype=tf.int32,shape=[])
        self.train_length = tf.placeholder(dtype=tf.int32)
        lstm_size = 1
        
        ob = U.get_placeholder(name="ob", dtype=tf.float32, shape=[None] + list(ob_space.shape))
        hidden_state_in = U.get_placeholder(name="h_state", dtype=tf.float32, shape=[None] + [lstm_size]) #states
        cell_state_in = U.get_placeholder(name="c_state", dtype=tf.float32, shape=[None] + [lstm_size]) #states
        
        # Create an LSTM from the basic Tensorflow LSTM module. This seemed like the simplest way
        # to create an LSTM.
        lstm = tf.contrib.rnn.BasicLSTMCell(lstm_size, state_is_tuple=True)
        
        # I think we need to be using a static evaluation of our LSTM, but I'm not certain.
        lstm_out, full_state_out = tf.nn.static_rnn(lstm, [ob], scope="polrnn", initial_state=(hidden_state_in, cell_state_in))
        hidden_state_out, cell_state_out = full_state_out
        lstm_out = lstm_out[0]
        
        # To create our value function, we will take the LSTM state and give it linear weights.
        vf = dense(lstm_out, 1, "vffc1", weight_init=U.normc_initializer(1.0))
        self.vpred = vf

        # To create our policy function, we will take the LSTM state and apply a single linear
        # combination, followed by a statistical sample from a normal distribution.
        if gaussian_fixed_var and isinstance(ac_space, gym.spaces.Box):
            mean = dense(lstm_out, pdtype.param_shape()[0]//2, "polfinal", U.normc_initializer(0.01))
            logstd = tf.get_variable(name="logstd", shape=[1, pdtype.param_shape()[0]//2], initializer=tf.zeros_initializer())
            pdparam = tf.concat([mean, mean * 0.0 + logstd], axis=1)
        else:
            pdparam = dense(lstm_out, pdtype.param_shape()[0], "polfinal", U.normc_initializer(0.01))

        self.pd = self.pdtype.pdfromflat(pdparam)

        # change for BC
        stochastic = U.get_placeholder(name="stochastic", dtype=tf.bool, shape=())
        ac = U.switch(stochastic, self.pd.sample(), self.pd.mode())
        self.ac = ac
        self.state = np.zeros(lstm_size, dtype=np.float32)
        self._act = U.function([stochastic, ob, hidden_state_in, cell_state_in],
            [ac, vf, hidden_state_out, cell_state_out])
        
        # Our policy object will store the most recent hidden and cell states, so when it needs
        # to act, it knows what the previous state was.
        self.hidden_state = np.zeros((1, lstm_size), dtype=np.float32)
        self.cell_state = np.zeros((1, lstm_size), dtype=np.float32)

    def act(self, stochastic, ob):

        # Every time we act, we also receive a value prediction and the state for the next
        # action.
        ac, vpred, new_hidden_state, new_cell_state = self._act(stochastic, ob[None],
            self.hidden_state, self.cell_state)

        old_hidden_state = self.hidden_state
        old_cell_state = self.cell_state
        
        self.hidden_state = new_hidden_state
        self.cell_state = new_cell_state

        # We need to return the old cell states so they can be included in the training dataset.
        return ac[0], vpred[0], old_hidden_state, old_cell_state

    def reset_state(self):
        self.hidden_state = np.zeros(self.hidden_state.shape, dtype=np.float32)
        self.cell_state = np.zeros(self.cell_state.shape, dtype=np.float32)

    def get_variables(self):
        return tf.get_collection(tf.GraphKeys.GLOBAL_VARIABLES, self.scope)
    
    def get_trainable_variables(self):
        return tf.get_collection(tf.GraphKeys.TRAINABLE_VARIABLES, self.scope)

