# RaceGym Minimal Boilerplate

A minimal reinforcement learning environment that connects a Python Gym environment to a C++ simulation built as a shared library (DLL) and launches a GLFW window when `render_mode='human'`.

## Prerequisites (Windows)
- Visual Studio 2022 (C++ Desktop workload)
- vcpkg installed. Either set `VCPKG_ROOT` or install to `C:\\vcpkg`.
- Python 3.10+

## Build the simulation DLL
From `sim/`:

```
build_sim.bat
```

This produces `sim\\build\\Release\\racegym_sim.dll`.

If vcpkg isn't in `C:\\vcpkg`, set an environment variable before building:

```
set VCPKG_ROOT=C:\\path\\to\\vcpkg
sim\\build_sim.bat
```

## Install Python deps
From repo root:

```
python -m pip install -r requirements.txt
python -m pip install -e .
```

## Usage

```python
import gymnasium as gym
import racegym

env = gym.make("RaceGym-v0", render_mode="human")  # or None for headless
obs, info = env.reset()
# ...
env.close()
```