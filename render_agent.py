import time
import gymnasium as gym
import numpy as np
from stable_baselines3 import PPO
from stable_baselines3.common.vec_env import DummyVecEnv, VecNormalize
from stable_baselines3.common.monitor import Monitor

from racegym.env import RaceGymEnv

def render_trained_agent(model_path="ppo_racegym_final.zip", num_episodes=5):
    """
    Load and render a trained PPO agent.
    
    Args:
        model_path: Path to the saved model file
        num_episodes: Number of episodes to render
    """
    # Create environment with human rendering
    env = RaceGymEnv(render_mode="human", fixed_start=True)
    env = Monitor(env)
    env = DummyVecEnv([lambda: env])
    env = VecNormalize.load("ppo_racegym_vecnormalize_final.pkl", env)  # Load normalization stats if available
    
    # Load the trained model
    print(f"Loading model from {model_path}...")
    model = PPO.load(model_path)
    
    print(f"Rendering {num_episodes} episodes...")
    
    for episode in range(num_episodes):
        obs = env.reset()
        done = np.array([False])
        episode_reward = 0.0
        step_count = 0
        
        print(f"\n--- Episode {episode + 1} ---")
        
        while not done[0]:
            # Get action from the trained model
            action, _states = model.predict(obs, deterministic=False)
            
            # Take action in environment
            obs, reward, done, info = env.step(action)
            episode_reward += reward[0]
            step_count += 1
                
            info_dict = info[0] if isinstance(info, (list, tuple)) else info
            if "lap_time" in info_dict:
                print(f"Previous lap time: {info_dict['lap_time']:.2f} seconds")
            if "total_distance" in info_dict:
                print(f"Total distance traveled: {info_dict['total_distance']:.2f} segments")
            
        
        print(f"Episode {episode + 1} finished:")
        print(f"  Steps: {step_count}")
        print(f"  Total Reward: {episode_reward:.2f}")
    
    env.close()
    print("\nRendering complete!")

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Render a trained RaceGym agent")
    parser.add_argument(
        "--model", 
        type=str, 
        default="ppo_racegym_final.zip",
        help="Path to the trained model file (default: ppo_racegym_final.zip)"
    )
    parser.add_argument(
        "--episodes", 
        type=int, 
        default=5,
        help="Number of episodes to render (default: 5)"
    )
    parser.add_argument(
        "--checkpoint",
        type=str,
        help="Path to a specific checkpoint (e.g., ./checkpoints/ppo_racegym_1100000_steps.zip)"
    )
    
    args = parser.parse_args()
    
    # Use checkpoint path if provided, otherwise use model path
    model_path = args.checkpoint if args.checkpoint else args.model
    
    render_trained_agent(model_path=model_path, num_episodes=args.episodes)
