import gymnasium as gym
import racegym
import time

def demo(render_mode):
    print(f"Starting demo with render_mode={render_mode!r}")
    env = gym.make("RaceGym-v0", render_mode=render_mode)
    obs, info = env.reset()
    print("Reset complete; sleeping briefly...")
    for step in range(300):
        action = env.action_space.sample()
        obs, reward, terminated, truncated, info = env.step(action)
        if render_mode is not None:
            env.render()
        time.sleep(1 / 60)
        if terminated or truncated:
            print(f"Episode ended after {step + 1} steps.")
            break
    env.close()
    print("Closed.")


if __name__ == "__main__":
    demo("human")
    demo(None)
