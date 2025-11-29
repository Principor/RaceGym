from .env import RaceGymEnv

__all__ = ["RaceGymEnv"]

try:
	from gymnasium.envs.registration import register

	# Register an id so users can do: gym.make("RaceGym-v0", render_mode=...)
	register(
		id="RaceGym-v0",
		entry_point="racegym.env:RaceGymEnv",
	)
except Exception:
	# Gymnasium not installed yet or registration failed; direct imports still work
	pass
