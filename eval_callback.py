import numpy as np
from stable_baselines3.common.callbacks import BaseCallback
from stable_baselines3.common.vec_env import VecEnv, sync_envs_normalization


class RaceGymEvalCallback(BaseCallback):
    """
    Custom callback for evaluating RaceGym agent at regular intervals.
    Logs total distance traveled per episode and lap times to TensorBoard.
    
    :param eval_env: The evaluation environment (can be vectorized or single env)
    :param n_eval_episodes: Number of episodes to run for each evaluation
    :param eval_freq: Evaluate every eval_freq training steps
    :param log_path: Path where to save evaluation logs (optional)
    :param deterministic: Whether to use deterministic actions during evaluation
    :param verbose: Verbosity level
    """
    
    def __init__(
        self,
        eval_env,
        n_eval_episodes: int = 5,
        eval_freq: int = 10000,
        log_path: str = None,
        deterministic: bool = True,
        verbose: int = 1,
    ):
        super().__init__(verbose)
        self.eval_env = eval_env
        self.n_eval_episodes = n_eval_episodes
        self.eval_freq = eval_freq
        self.log_path = log_path
        self.deterministic = deterministic
        self.evaluations_timesteps = []
        self.evaluations_results = []
        
        # Check if vectorized
        self.is_vec_env = isinstance(eval_env, VecEnv)
        if not self.is_vec_env:
            # Wrap in a dummy vec env for consistency
            from stable_baselines3.common.vec_env import DummyVecEnv
            self.eval_env = DummyVecEnv([lambda: eval_env])
    
    def _on_step(self) -> bool:
        # Evaluate at regular intervals
        if self.n_calls % self.eval_freq == 0:
            self._evaluate()
        return True
    
    def _evaluate(self):
        """Run evaluation episodes and log metrics to TensorBoard"""
        if self.verbose > 0:
            print(f"Evaluating at step {self.num_timesteps}...")
        
        # Sync training and eval env if there's normalization
        if self.model.get_vec_normalize_env() is not None:
            try:
                sync_envs_normalization(self.training_env, self.eval_env)
            except AttributeError:
                pass
        
        episode_distances = []
        all_lap_times = []
        episode_rewards = []
        episode_lengths = []
        
        episodes_completed = 0
        obs = self.eval_env.reset()
        
        while episodes_completed < self.n_eval_episodes:
            action, _ = self.model.predict(obs, deterministic=self.deterministic)
            obs, rewards, dones, infos = self.eval_env.step(action)
            
            # Check for episode completion
            for i, done in enumerate(dones):
                if done:
                    episodes_completed += 1
                    
                    # Extract info dict for this environment
                    if self.is_vec_env or len(infos) > 1:
                        info = infos[i]
                    else:
                        info = infos[0] if isinstance(infos, list) else infos
                    
                    # Log total distance if episode ended
                    if 'total_distance' in info:
                        episode_distances.append(info['total_distance'])
                    
                    # Log episode reward and length from Monitor wrapper
                    if 'episode' in info:
                        episode_rewards.append(info['episode']['r'])
                        episode_lengths.append(info['episode']['l'])
                    
            # Collect lap times from all environments
            for info_dict in (infos if isinstance(infos, list) else [infos]):
                if 'lap_time' in info_dict:
                    all_lap_times.append(info_dict['lap_time'])
        
        # Calculate statistics
        mean_distance = np.mean(episode_distances) if episode_distances else 0.0
        std_distance = np.std(episode_distances) if episode_distances else 0.0
        
        mean_lap_time = np.mean(all_lap_times) if all_lap_times else 0.0
        std_lap_time = np.std(all_lap_times) if all_lap_times else 0.0
        min_lap_time = np.min(all_lap_times) if all_lap_times else 0.0
        
        mean_reward = np.mean(episode_rewards) if episode_rewards else 0.0
        mean_ep_length = np.mean(episode_lengths) if episode_lengths else 0.0
        
        # Log to TensorBoard
        self.logger.record("eval/mean_distance", mean_distance)
        self.logger.record("eval/std_distance", std_distance)
        self.logger.record("eval/mean_reward", mean_reward)
        self.logger.record("eval/mean_ep_length", mean_ep_length)
        
        if all_lap_times:
            self.logger.record("eval/mean_lap_time", mean_lap_time)
            self.logger.record("eval/std_lap_time", std_lap_time)
            self.logger.record("eval/min_lap_time", min_lap_time)
            self.logger.record("eval/n_laps_completed", len(all_lap_times))
        
        # Store for later analysis
        self.evaluations_timesteps.append(self.num_timesteps)
        self.evaluations_results.append({
            'mean_distance': mean_distance,
            'std_distance': std_distance,
            'mean_lap_time': mean_lap_time,
            'std_lap_time': std_lap_time,
            'min_lap_time': min_lap_time,
            'n_laps': len(all_lap_times),
            'mean_reward': mean_reward,
        })
        
        if self.verbose > 0:
            print(f"Eval results at step {self.num_timesteps}:")
            print(f"  Mean distance: {mean_distance:.2f} ± {std_distance:.2f}")
            print(f"  Mean reward: {mean_reward:.2f}")
            if all_lap_times:
                print(f"  Mean lap time: {mean_lap_time:.2f} ± {std_lap_time:.2f} steps")
                print(f"  Best lap time: {min_lap_time:.2f} steps")
                print(f"  Total laps completed: {len(all_lap_times)}")
