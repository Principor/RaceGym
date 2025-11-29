import gymnasium as gym
import racegym
import time

def demo(render_mode):
    print(f"Starting demo with render_mode={render_mode!r}")
    env = gym.make("RaceGym-v0", render_mode=render_mode)
    obs, info = env.reset()
    print("Reset complete; sleeping briefly...")
    # Let the window stay up briefly if human
    time.sleep(2.0)
    env.close()
    print("Closed.")


if __name__ == "__main__":
    demo("human")
    demo(None)
