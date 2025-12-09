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
        # 40 waypoints (local x,z) => 80 values, plus longitudinal vel, lateral vel, yaw rate => 83
        self.observation_space = spaces.Box(low=-np.inf, high=np.inf, shape=(83,), dtype=np.float32)
        self.action_space = spaces.Box(low=-1.0, high=1.0, shape=(2,), dtype=np.float32)

        self._dll: ctypes.CDLL | None = None
        self._sim_context: ctypes.c_void_p | None = None
        self._vehicle: ctypes.c_void_p | None = None
        self._track_length: int = 0
        self._last_track_position: float = 0.0
        
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
        self._dll.sim_load_track.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        self._dll.sim_load_track.restype = None
        self._dll.sim_add_vehicle.argtypes = [ctypes.c_void_p, ctypes.c_float]
        self._dll.sim_add_vehicle.restype = ctypes.c_void_p
        self._dll.sim_remove_vehicle.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self._dll.sim_remove_vehicle.restype = None
        self._dll.sim_set_vehicle_control.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_float, ctypes.c_float]
        self._dll.sim_set_vehicle_control.restype = None
        self._dll.sim_get_vehicle_track_position.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self._dll.sim_get_vehicle_track_position.restype = ctypes.c_float
        self._dll.sim_get_track_length.argtypes = [ctypes.c_void_p]
        self._dll.sim_get_track_length.restype = ctypes.c_int
        self._dll.sim_is_vehicle_off_track.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self._dll.sim_is_vehicle_off_track.restype = ctypes.c_int
        self._dll.sim_get_observation.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_float), ctypes.c_int]
        self._dll.sim_get_observation.restype = ctypes.c_int
        self._dll.sim_get_vehicle_velocity.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float)]
        self._dll.sim_get_vehicle_velocity.restype = None
        self._dll.sim_get_track_normal.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.POINTER(ctypes.c_float)]
        self._dll.sim_get_track_normal.restype = None

    def _load_track(self, name: str):
        if self._dll is None or self._sim_context is None:
            raise RuntimeError("DLL or simulation context not initialized")
        path = Path(__file__).resolve().parents[1] / "tracks" / f"{name}.json"
        if not path.is_file():
            raise FileNotFoundError(f"Track file not found: {path}")
        encoded = str(path).encode('utf-8')
        self._dll.sim_load_track(self._sim_context, encoded)

    def reset(self, *, seed: int | None = None, options: dict | None = None):
        super().reset(seed=seed)
        self._load_track("track1")
        if self._vehicle is not None:
            self._dll.sim_remove_vehicle(self._sim_context, self._vehicle)
        spawn_point = self.np_random.uniform(0.0,  self._dll.sim_get_track_length(self._sim_context))
        self._vehicle = self._dll.sim_add_vehicle(self._sim_context, spawn_point)
        self._track_length = self._dll.sim_get_track_length(self._sim_context)
        self._last_track_position = spawn_point
        obs = self._get_observation()
        info = {}
        return obs, info

    def step(self, action):
        if self._dll is not None and self._sim_context is not None:
            self._dll.sim_set_vehicle_control(self._vehicle, ctypes.c_float(action[0]), ctypes.c_float(action[1]), ctypes.c_float(-action[1]))
            self._dll.sim_step(self._sim_context)
        
        # Get current position along track
        current_track_position = self._dll.sim_get_vehicle_track_position(self._sim_context, self._vehicle)
        
        # Calculate reward with wraparound handling
        delta = current_track_position - self._last_track_position
        
        # Handle wraparound at track boundaries
        if delta > self._track_length / 2:
            # Wrapped backwards (e.g., from 0.1 to 9.9 on a 10-segment track)
            delta -= self._track_length
        elif delta < -self._track_length / 2:
            # Wrapped forwards (e.g., from 9.9 to 0.1 on a 10-segment track)
            delta += self._track_length
        
        reward = delta
        self._last_track_position = current_track_position
        
        # Check if vehicle is off track
        terminated = False
        is_off_track = self._dll.sim_is_vehicle_off_track(self._sim_context, self._vehicle)
        if(is_off_track != 0):
            terminated = True
            
            # Get velocity-projected penalty
            # Get current track position along curve
            current_track_position = self._dll.sim_get_vehicle_track_position(self._sim_context, self._vehicle)
            
            # Get vehicle velocity (3D vector) -> project to 2D (XZ plane)
            vel_buf = (ctypes.c_float * 3)()
            self._dll.sim_get_vehicle_velocity(self._vehicle, vel_buf)
            vel_2d = np.array([vel_buf[0], vel_buf[2]], dtype=np.float32)
            
            # Get track normal at current position (2D)
            normal_buf = (ctypes.c_float * 2)()
            self._dll.sim_get_track_normal(self._sim_context, current_track_position, normal_buf)
            track_normal = np.array([normal_buf[0], normal_buf[1]], dtype=np.float32)
            
            # Project velocity onto the track normal
            # Positive component means moving away from track
            normal_velocity = np.dot(vel_2d, track_normal)
            
            # Apply penalty proportional to the outward velocity component
            # Max of 0 ensures we only penalize when moving away from track
            penalty = abs(normal_velocity)
            reward -= penalty

        obs = self._get_observation()
        truncated = False
        info = {'track_position': current_track_position, 'off_track': terminated}
        return obs, reward, terminated, truncated, info

    def _get_observation(self) -> np.ndarray:
        buf_len = 83
        buf_type = ctypes.c_float * buf_len
        buf = buf_type()
        count = self._dll.sim_get_observation(self._sim_context, self._vehicle, buf, buf_len)
        return np.frombuffer(buf, dtype=np.float32, count=count)

    def render(self):
        # Window is handled by the sim itself in 'human' mode.
        return None

    def close(self):
        if self._dll is not None and self._sim_context is not None:
            self._dll.sim_shutdown(self._sim_context)
            self._sim_context = None

