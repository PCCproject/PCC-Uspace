import sys

if not hasattr(sys, 'argv'):
    sys.argv = ['']

class ModelParameterSet:
    _default_history_len = 3
    _default_hidden_layers = 3
    _default_hidden_size = 32
    _default_ts_per_batch = 4096
    _default_max_kl = 0.0001
    _default_cg_iters = 10
    _default_cg_damping = 1e-3
    _default_gamma = 0.0
    _default_lam = 1.0
    _default_vf_iters = 3
    _default_vf_stepsize = 1e-4
    _default_entcoeff = 0.0

    def __init__(self, name, path, args=sys.argv):
        self.name = name
        self.path = path

        self.history_len = ModelParameterSet._default_history_len
        self.hidden_layers = ModelParameterSet._default_hidden_layers
        self.hidden_size = ModelParameterSet._default_hidden_size
        self.ts_per_batch = ModelParameterSet._default_ts_per_batch
        self.max_kl = ModelParameterSet._default_max_kl
        self.cg_iters = ModelParameterSet._default_cg_iters
        self.cg_damping = ModelParameterSet._default_cg_damping
        self.gamma = ModelParameterSet._default_gamma
        self.lam = ModelParameterSet._default_lam
        self.vf_iters = ModelParameterSet._default_vf_iters
        self.vf_stepsize = ModelParameterSet._default_vf_stepsize
        self.entcoeff = ModelParameterSet._default_entcoeff

        self.update_from_args(args)

    def update_from_args(self, args):
        for arg in args:
            arg_val = "NULL"
            try:
                arg_val = float(arg[arg.rfind("=") + 1:])
            except:
                pass

            if "--history-len=" in arg:
                self.history_len = int(arg_val)

            if "--hid-layers=" in arg:
                self.hidden_layers = int(arg_val)

            if "--hid-size=" in arg:
                self.hidden_size = int(arg_val)

            if "--ts-per-batch=" in arg:
                self.ts_per_batch = int(arg_val)

            if "--max-kl=" in arg:
                self.max_kl = arg_val

            if "--cg-iters=" in arg:
                self.cg_iters = int(arg_val)

            if "--cg-damping=" in arg:
                self.cg_damping = arg_val

            if "--gamma=" in arg:
                self.gamma = arg_val

            if "--lambda=" in arg:
                self.lam = arg_val

            if "--vf-iters=" in arg:
                self.vf_iters = int(arg_val)

            if "--vf-stepsize=" in arg:
                self.vf_stepsize = arg_val

            if "--entcoeff=" in arg:
                self.entcoeff = arg_val
            
