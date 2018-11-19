import sys
from rl_helpers.simple_arg_parse import arg_or_default

if not hasattr(sys, 'argv'):
    sys.argv = ['']


##
#   The ModelParameterSet is a single place to store all of the model parameters. Using this
#   structure directly requires edits to the policy functions, but the training helper can
#   translate some of these parameters for us.
##
class ModelParameterSet:

    # Default model parameters.
    _default_history_len = 10
    _default_hidden_layers = 3
    _default_hidden_size = 32
    _default_ts_per_batch = 8192
    _default_max_kl = 0.0001
    _default_cg_iters = 10
    _default_cg_damping = 1e-3
    _default_gamma = 0.9
    _default_lam = 1.0
    _default_vf_iters = 3
    _default_vf_stepsize = 1e-5
    _default_entcoeff = 0.0

    def __init__(self, name, path, args=sys.argv):
        # On __init__, we will check all of the command line arguments for any changes to the
        # model parameters.

        self.name = name
        self.path = path

        self.history_len = arg_or_default("--history-len=",
            ModelParameterSet._default_history_len)
        self.hidden_layers = arg_or_default("--hid-layers=",
            ModelParameterSet._default_hidden_layers)
        self.hidden_size = arg_or_default("--hid-size=",
            ModelParameterSet._default_hidden_size)
        self.ts_per_batch = arg_or_default("--ts-per-batch=",
            ModelParameterSet._default_ts_per_batch)
        self.max_kl = arg_or_default("--max-kl=",
            ModelParameterSet._default_max_kl)
        self.cg_iters = arg_or_default("--cg-iters=",
            ModelParameterSet._default_cg_iters)
        self.cg_damping = arg_or_default("--cg-damping=",
            ModelParameterSet._default_cg_damping)
        self.gamma = arg_or_default("--gamma=",
            ModelParameterSet._default_gamma)
        self.lam = arg_or_default("--lambda=",
            ModelParameterSet._default_lam)
        self.vf_iters = arg_or_default("--vf-iters=",
            ModelParameterSet._default_vf_iters)
        self.vf_stepsize = arg_or_default("--vf-stepsize=",
            ModelParameterSet._default_vf_stepsize)
        self.entcoeff = arg_or_default("--entcoeff=",
            ModelParameterSet._default_entcoeff)
