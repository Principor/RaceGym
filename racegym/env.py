import os
import sys
import time
import ctypes
from pathlib import Path

import numpy as np
import gymnasium as gym
from gymnasium import spaces


def _find_sim_dll() -> Path | None:
    repo_root = Path(__file__).resolve().parents[1]
    candidates = [
        repo_root / "sim" / "build" / "Release" / "racegym_sim.dll",
        repo_root / "sim" / "build" / "Debug" / "racegym_sim.dll",
        repo_root / "sim" / "build" / "racegym_sim.dll",
    ]
    for p in candidates:
        if p.is_file():
            return p
    return None


class RaceGymEnv(gym.Env):
    metadata = {"render_modes": ["human", None], "render_fps": 60}

    def __init__(self, render_mode: str | None = None):
        assert render_mode in ("human", None), "render_mode must be 'human' or None"
        self.render_mode = render_mode
        self.observation_space = spaces.Box(low=-np.inf, high=np.inf, shape=(1,), dtype=np.float32)
        self.action_space = spaces.Box(low=-1.0, high=1.0, shape=(1,), dtype=np.float32)

        self._dll: ctypes.CDLL | None = None
        self._sim_context: ctypes.c_void_p | None = None

    def _load_dll(self):
        if self._dll is not None:
            return
        dll_path = _find_sim_dll()
        if dll_path is None:
            raise FileNotFoundError(
                "racegym_sim.dll not found. Build it with sim\\build_sim.bat or set RACEGYM_SIM_DLL."
            )
        self._dll = ctypes.CDLL(str(dll_path))
        # prototypes
        self._dll.sim_init.argtypes = [ctypes.c_int]
        self._dll.sim_init.restype = ctypes.c_void_p
        self._dll.sim_step.argtypes = [ctypes.c_void_p]
        self._dll.sim_step.restype = None
        self._dll.sim_shutdown.argtypes = [ctypes.c_void_p]
        self._dll.sim_shutdown.restype = None

    def reset(self, *, seed: int | None = None, options: dict | None = None):
        super().reset(seed=seed)
        self._load_dll()
        if self._sim_context is not None:
            self._dll.sim_shutdown(self._sim_context)
            self._sim_context = None
            # Give the DLL a moment to clean up
            time.sleep(0.05)
        windowed = 1 if self.render_mode == "human" else 0
        self._sim_context = self._dll.sim_init(windowed)
        if self._sim_context is None:
            raise RuntimeError("sim_init failed - returned null context")
        obs = np.zeros((1,), dtype=np.float32)
        info = {}
        return obs, info

    def step(self, action):
        if self._dll is not None and self._sim_context is not None:
            self._dll.sim_step(self._sim_context)
        obs = np.zeros((1,), dtype=np.float32)
        reward = 0.0
        terminated = False
        truncated = False
        info = {}
        return obs, reward, terminated, truncated, info

    def render(self):
        # Window is handled by the sim itself in 'human' mode.
        return None

    def close(self):
        if self._dll is not None and self._sim_context is not None:
            self._dll.sim_shutdown(self._sim_context)
            self._sim_context = None

