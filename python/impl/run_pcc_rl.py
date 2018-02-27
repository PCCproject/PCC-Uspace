from mpi4py import MPI
from baselines.common import set_global_seeds
import os.path as osp
import gym
import logging
from baselines_master import logger
from baselines_master import bench
from impl.pcc_wrappers import make_pcc


def train(env_id, num_timesteps, seed):
    from impl.policies.basic_nn import BasicNNPolicy
    from impl.policies.nosharing_cnn_policy import CnnPolicy
    # from baselines_master.trpo_mpi import trpo_mpi_for_log
    from impl.trpo_mpi import trpo_mpi_for_log
    import baselines.common.tf_util as U
    rank = MPI.COMM_WORLD.Get_rank()
    sess = U.single_threaded_session()
    sess.__enter__()
    if rank == 0:
        logger.configure()
    else:
        logger.configure(format_strs=[])

    workerseed = seed + 10000 * MPI.COMM_WORLD.Get_rank()
    set_global_seeds(workerseed)
    env = make_pcc(env_id)

    def policy_fn(name, ob_space, ac_space): #pylint: disable=W0613
        return BasicNNPolicy(name=name, ob_space=env.observation_space, ac_space=env.action_space)
    env = bench.Monitor(env, logger.get_dir() and osp.join(logger.get_dir(), str(rank)))
    env.seed(workerseed)
    gym.logger.setLevel(logging.WARN)

    # env = wrap_deepmind(env)
    env.seed(workerseed)

    trpo_mpi_for_log.learn(env, policy_fn, timesteps_per_batch=512, max_kl=0.001, cg_iters=10, cg_damping=1e-3,
        max_timesteps=int(num_timesteps * 1.1), gamma=0.98, lam=1.0, vf_iters=3, vf_stepsize=1e-4, entcoeff=0.00)
    env.close()


def main():
    import argparse
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    # parser.add_argument('--env', help='environment ID', default='PongNoFrameskip-v4')
    parser.add_argument('--env', help='environment ID', default='PCCEnv-v0')
    parser.add_argument('--seed', help='RNG seed', type=int, default=0)
    parser.add_argument('--num-timesteps', type=int, default=int(10e6))
    args = parser.parse_args()
    train(args.env, num_timesteps=args.num_timesteps, seed=args.seed)


if __name__ == "__main__":
    main()
