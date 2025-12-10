import gymnasium as gym
import numpy as np
from stable_baselines3 import PPO
from stable_baselines3.common.vec_env import SubprocVecEnv
from stable_baselines3.common.monitor import Monitor
from stable_baselines3.common.callbacks import CheckpointCallback, EvalCallback, CallbackList

from racegym.env import RaceGymEnv

def make_env(render_mode=None):
    def _init():
        env = RaceGymEnv(render_mode=render_mode)
        env = Monitor(env)
        return env
    return _init

if __name__ == "__main__":
    # VecEnv wrapper (single env)
    vec_env = SubprocVecEnv([make_env(render_mode=None) for _ in range(4)])

    # PPO hyperparameters starter
    model = PPO(
        "MlpPolicy",
        vec_env,
        n_steps=8192,
        batch_size=2048,
        gae_lambda=0.95,
        gamma=0.99,
        n_epochs=10,
        learning_rate=3e-4,
        clip_range=0.2,
        ent_coef=0.0,
        vf_coef=0.5,
        verbose=1,
    )

    # Checkpoints every 100k steps
    checkpoint_cb = CheckpointCallback(save_freq=100_000, save_path="./checkpoints", name_prefix="ppo_racegym")

    total_steps = 20_000_000
    model.learn(total_timesteps=total_steps, callback=checkpoint_cb)
    model.save("ppo_racegym_final")
    vec_env.close()