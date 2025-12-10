import time
import gymnasium as gym
import numpy as np
from stable_baselines3 import PPO

from racegym.env import RaceGymEnv

def render_trained_agent(model_path="ppo_racegym_final.zip", num_episodes=5):
    """
    Load and render a trained PPO agent.
    
    Args:
        model_path: Path to the saved model file
        num_episodes: Number of episodes to render
    """
    # Create environment with human rendering
    env = RaceGymEnv(render_mode="human")
    
    # Load the trained model
    print(f"Loading model from {model_path}...")
    model = PPO.load(model_path)
    
    print(f"Rendering {num_episodes} episodes...")
    
    for episode in range(num_episodes):
        obs, info = env.reset()
        done = False
        truncated = False
        episode_reward = 0
        step_count = 0
        
        print(f"\n--- Episode {episode + 1} ---")
        
        while not (done or truncated):
            # Get action from the trained model
            action, _states = model.predict(obs, deterministic=True)
            
            # Take action in environment
            obs, reward, done, truncated, info = env.step(action)
            episode_reward += reward
            step_count += 1
        
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
