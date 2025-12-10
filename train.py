import gymnasium as gym
import numpy as np
from stable_baselines3 import PPO
from stable_baselines3.common.vec_env import SubprocVecEnv
from stable_baselines3.common.monitor import Monitor
from stable_baselines3.common.callbacks import CheckpointCallback, CallbackList

from racegym.env import RaceGymEnv
from eval_callback import RaceGymEvalCallback

def make_env(render_mode=None):
    def _init():
        env = RaceGymEnv(render_mode=render_mode)
        env = Monitor(env)
        return env
    return _init

if __name__ == "__main__":
    # VecEnv wrapper (training envs)
    vec_env = SubprocVecEnv([make_env(render_mode=None) for _ in range(4)])
    
    # Create evaluation environment (single env)
    eval_env = make_env(render_mode=None)()

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
        tensorboard_log="./tensorboard_logs/",
    )

    # Checkpoints every 100k steps
    checkpoint_cb = CheckpointCallback(
        save_freq=100_000,
        save_path="./checkpoints",
        name_prefix="ppo_racegym"
    )
    
    # Custom evaluation callback
    eval_cb = RaceGymEvalCallback(
        eval_env=eval_env,
        n_eval_episodes=5,
        eval_freq=100_000,
        deterministic=True,
        verbose=1,
    )
    
    # Combine callbacks
    callback = CallbackList([checkpoint_cb, eval_cb])

    total_steps = 20_000_000
    model.learn(total_timesteps=total_steps, callback=callback)
    model.save("ppo_racegym_final")
    vec_env.close()
    eval_env.close()