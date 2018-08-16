import numpy as np                                                                               
import tensorflow as tf                                                                          
import gym
import open_ai.common.tf_util as U
from open_ai.common.tf_util import dense
from open_ai.common.distributions import make_pdtype

"""
class MlpPolicy(object):
    recurrent = False

    def __init__(self, name, reuse=False, *args, **kwargs):
        with tf.variable_scope(name):
            if reuse:
                tf.get_variable_scope().reuse_variables()
            self._init(*args, **kwargs)
            self.scope = tf.get_variable_scope().name

    def _init(self, ob_space, ac_space, hid_size, num_hid_layers, gaussian_fixed_var=True):
        assert isinstance(ob_space, gym.spaces.Box)

        self.pdtype = pdtype = make_pdtype(ac_space)
        sequence_length = None

        ob = U.get_placeholder(name="ob", dtype=tf.float32, shape=[sequence_length] + list(ob_space.shape))

        last_out = ob
        for i in range(num_hid_layers):
            last_out = tf.nn.relu(dense(last_out, hid_size, "vffc%i" % (i+1), weight_init=U.normc_initializer(1.0)))
        self.vpred = dense(last_out, 1, "vffinal", weight_init=U.normc_initializer(1.0))[:, 0]

        last_out = ob
        for i in range(num_hid_layers):
            last_out = tf.nn.relu(dense(last_out, hid_size, "polfc%i" % (i+1), weight_init=U.normc_initializer(1.0)))

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
        return ac1[0], vpred1[0]

    def get_variables(self):

def lstm(xs, ms, s, scope, nh, init_scale=1.0):
    nbatch = 1
    nin = [v.value for v in xs[0].get_shape()]
    print("Nin = %s" % str(nin))
    nsteps = len(xs)
    with tf.variable_scope(scope):
        #wx = tf.get_variable("wx", [nin, nh*4], initializer=ortho_init(init_scale))
        #wh = tf.get_variable("wh", [nh, nh*4], initializer=ortho_init(init_scale))
        wx = tf.get_variable("wx", [x.shape[-1], nh*4], initializer=ortho_init(init_scale))
        wh = tf.get_variable("wh", [nh, nh*4], initializer=ortho_init(init_scale))
        b = tf.get_variable("b", [nh*4], initializer=tf.constant_initializer(0.0))

    c, h = s
    for idx, (x, m) in enumerate(zip(xs, ms)):
        c = c*(1-m)
        h = h*(1-m)
        z = tf.matmul(x, wx) + tf.matmul(h, wh) + b
        i, f, o, u = tf.split(axis=1, num_or_size_splits=4, value=z)
        i = tf.nn.sigmoid(i)
        f = tf.nn.sigmoid(f)
        o = tf.nn.sigmoid(o)
        u = tf.tanh(u)
        c = f*c + i*u
        h = o*tf.tanh(c)
        xs[idx] = h
    s = (c, h)
    return xs, s
#"""

"""
"""

def ortho_init(scale=1.0):
    def _ortho_init(shape, dtype, partition_info=None):
        #lasagne ortho init for tf
        shape = tuple(shape)
        if len(shape) == 2:
            flat_shape = shape
        elif len(shape) == 4: # assumes NHWC
            flat_shape = (np.prod(shape[:-1]), shape[-1])
        else:
            raise NotImplementedError
        a = np.random.normal(0.0, 1.0, flat_shape)
        u, _, v = np.linalg.svd(a, full_matrices=False)
        q = u if u.shape == flat_shape else v # pick the one with the correct shape
        q = q.reshape(shape)
        return (scale * q[:shape[0], :shape[1]]).astype(np.float32)
    return _ortho_init

#"""
def lstm(inputs, hidden_state_in, cell_state_in, scope, lstm_size, init_scale=1.0):
    mult_width = 4 * lstm_size
    with tf.variable_scope(scope):
        #wx = tf.get_variable("wx", [nin, nh*4], initializer=ortho_init(init_scale))
        #wh = tf.get_variable("wh", [nh, nh*4], initializer=ortho_init(init_scale))
        wx = tf.get_variable("wx", [inputs.shape[1], mult_width], initializer=ortho_init(init_scale))
        wh = tf.get_variable("wh", [lstm_size, mult_width], initializer=ortho_init(init_scale))
        b = tf.get_variable("b", [mult_width], initializer=tf.constant_initializer(0.0))

    c = cell_state_in
    h = hidden_state_in
    #print("h shape = %s" % str(h.shape))
    #print("inputs shape = %s" % str(inputs.shape))
    #z = tf.matmul(tf.reshape(inputs, shape=(1, inputs.shape[0])), wx) + tf.matmul(tf.reshape(h, (1, h.shape[0])), wh) + b
    z = tf.matmul(inputs, wx) + tf.matmul(h, wh) + b
    i, f, o, u = tf.split(axis=1, num_or_size_splits=4, value=z)
    i = tf.nn.sigmoid(i)
    f = tf.nn.sigmoid(f)
    o = tf.nn.sigmoid(o)
    u = tf.tanh(u)
    c = f*c + i*u
    h = o*tf.tanh(c)
    #output = h
    #state = (c, h)
    return h, c
#"""

class LstmPolicy(object):

    def __init__(self, name, reuse=False, *args, **kwargs):
        with tf.variable_scope(name):
            if reuse:
                tf.get_variable_scope().reuse_variables()
            self._init(*args, **kwargs)
            self.scope = tf.get_variable_scope().name

    def _init(self, ob_space, ac_space, reuse=False, gaussian_fixed_var=False):
        nenv = 1
        nbatch = 1

        self.pdtype = pdtype = make_pdtype(ac_space)

        #nh, nw, nc = ob_space.shape
        #nh = 1
        #nc = 1
        #ob_shape = (nbatch, nh, nw, nc)
        #nact = ac_space.n
        self.batch_size = tf.placeholder(dtype=tf.int32,shape=[])
        self.train_length = tf.placeholder(dtype=tf.int32)
        lstm_size = 32#ob_space.shape[0]
        
        #ob = U.get_placeholder(name="ob", dtype=tf.float32, shape=[1, 1, lstm_size])
        #tf.placeholder(tf.float32, ob_shape) #obs
        #M = tf.placeholder(tf.float32, [nbatch]) #mask (done t-1)
        #S = (tf.placeholder(tf.float32, [lstm_size]), #states
        #        tf.placeholder(tf.float32, [lstm_size])) #states
       
        #print("Hidden state input shape = " + str(hidden_state_in.shape))
        #print("Cell state input shape = " + str(cell_state_in.shape))
        #print("Observation shape = " + str(self.ob.shape))
        #print("Altered ob shape = " + str([None] + list(ob_space.shape)))

        #xs = batch_to_seq(X, 1, 1)
        #ms = batch_to_seq(M, 1, 1)
        #self.batch_size = tf.placeholder(dtype=tf.int32,shape=[])
        
        #"""
        ob = U.get_placeholder(name="ob", dtype=tf.float32, shape=[None] + list(ob_space.shape))
        cell_state_in = (tf.placeholder(tf.float32, [1, lstm_size]), #states
                tf.placeholder(tf.float32, [1, lstm_size])) #states
        lstm = tf.contrib.rnn.BasicLSTMCell(lstm_size, state_is_tuple=True)
        lstm_out, cell_state_out = tf.nn.static_rnn(lstm, [ob], initial_state=cell_state_in)
        #lstm_out, cell_state_out = tf.nn.dynamic_rnn(lstm, ob, initial_state=cell_state_in)
        lstm_out = lstm_out[0]
        """
        self.ob = U.get_placeholder(name="ob", dtype=tf.float32, shape=[None] + list(ob_space.shape))
        hidden_state_in = U.get_placeholder(name="h_state", dtype=tf.float32, shape=[None] + [lstm_size])
        cell_state_in = U.get_placeholder(name="c_state", dtype=tf.float32, shape=[None] + [lstm_size])
        
        hidden_state_out, cell_state_out = lstm(self.ob, hidden_state_in, cell_state_in, 'pollstm', lstm_size)
        
        #print("Hidden state output shape = " + str(hidden_state_out.shape))
        #print("Cell state output shape = " + str(cell_state_out.shape))
        vf = dense(hidden_state_out, 1, "vffc1", weight_init=U.normc_initializer(1.0))
        
        """
        #h5 = seq_to_batch(h5)
        print(lstm_out)
        vf = dense(lstm_out, 1, "vffc1", weight_init=U.normc_initializer(1.0))
        self.vpred = vf

        if gaussian_fixed_var and isinstance(ac_space, gym.spaces.Box):
            mean = dense(hidden_state_out, pdtype.param_shape()[0]//2, "polfinal", U.normc_initializer(0.01))
            logstd = tf.get_variable(name="logstd", shape=[1, pdtype.param_shape()[0]//2], initializer=tf.zeros_initializer())
            pdparam = tf.concat([mean, mean * 0.0 + logstd], axis=1)
        else:
            pdparam = dense(hidden_state_out, pdtype.param_shape()[0], "polfinal", U.normc_initializer(0.01))

        self.pd = self.pdtype.pdfromflat(pdparam)

        # change for BC
        stochastic = U.get_placeholder(name="stochastic", dtype=tf.bool, shape=())
        ac = U.switch(stochastic, self.pd.sample(), self.pd.mode())
        self.ac = ac
        self.state = np.zeros(lstm_size, dtype=np.float32)
        self._act = U.function([stochastic, self.ob, hidden_state_in, cell_state_in],
            [ac, vf, hidden_state_out, cell_state_out])
        
        #
        #self.pd = self.pdtype.pdfromflat(pi)
        #
        #v0 = vf[:, 0]
        #a0 = self.pd.sample()
        #neglogp0 = self.pd.neglogp(a0)
        self.hidden_state = np.zeros((1, lstm_size), dtype=np.float32)
        self.cell_state = np.zeros((1, lstm_size), dtype=np.float32)
        #print("Hidden state shape = " + str(self.hidden_state.shape))
        #print("Cell state shape = " + str(self.cell_state.shape))
        #self.state = self.initial_state

        #self.X = X
        #self.M = M
        #self.S = S
        #self.pi = pi
        #self.vf = vf

    def act(self, stochastic, ob):
        #print("ob shape = " + str(ob.shape))
        #print("ob[None] shape = " + str(ob[None].shape))
        ac, vpred, new_hidden_state, new_cell_state = self._act(stochastic, ob[None], self.hidden_state, self.cell_state)
        
        old_hidden_state = self.hidden_state
        old_cell_state = self.cell_state
        
        self.hidden_state = new_hidden_state
        self.cell_state = new_cell_state
        return ac[0], vpred[0], old_hidden_state, old_cell_state
    
    def get_variables(self):
        return tf.get_collection(tf.GraphKeys.GLOBAL_VARIABLES, self.scope)
    
    def get_trainable_variables(self):
        return tf.get_collection(tf.GraphKeys.TRAINABLE_VARIABLES, self.scope)

