#ifndef RACEGYM_SIM_H
#define RACEGYM_SIM_H

#ifdef _WIN32
#define RACEGYM_API __declspec(dllexport)
#else
#define RACEGYM_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize a new simulation instance.
 * 
 * @param windowed If non-zero, creates a window for rendering
 * @return Opaque pointer to simulation context, or nullptr on failure
 */
RACEGYM_API void* sim_init(int windowed);

/**
 * Step the simulation forward by one frame.
 * If windowed mode is enabled, this also handles rendering and event polling.
 * 
 * @param sim_context Pointer to simulation context returned by sim_init
 */
RACEGYM_API void sim_step(void* sim_context);

/**
 * Shutdown and clean up a simulation instance.
 * 
 * @param sim_context Pointer to simulation context returned by sim_init
 */
RACEGYM_API void sim_shutdown(void* sim_context);

/**
 * Load a track from a JSON file into the simulation.
 * 
 * @param sim_context Pointer to simulation context returned by sim_init
 * @param path Path to the JSON file containing track data
 * @return 0 on success, non-zero on failure
 */
RACEGYM_API void sim_load_track(void* sim_context, const char* path);

/**
 * Add a vehicle to the simulation at the default starting position.
 * 
 * @param sim_context Pointer to simulation context returned by sim_init
 */
RACEGYM_API void* sim_add_vehicle(void* sim_context, float spawn_t);

/**
 * Remove a vehicle from the simulation.
 * 
 * @param sim_context Pointer to simulation context returned by sim_init
 * @param vehicle Pointer to the vehicle returned by sim_add_vehicle
 */
RACEGYM_API void sim_remove_vehicle(void* sim_context, void* vehicle);

/**
 * Set control inputs for the specified vehicle.
 * 
 * @param vehicle Pointer to the vehicle returned by sim_add_vehicle
 * @param steer Steering input in range [-1.0, 1.0]
 * @param throttle Throttle input in range [0.0, 1.0]
 * @param brake Brake input in range [0.0, 1.0]
 */
RACEGYM_API void sim_set_vehicle_control(void* vehicle, float steer, float throttle, float brake);

/**
 * Get the vehicle's position along the track curve.
 * 
 * @param sim_context Pointer to simulation context
 * @param vehicle_ptr Pointer to the vehicle
 * @return Position along track curve in range [0, num_segments)
 */
RACEGYM_API float sim_get_vehicle_track_position(void* sim_context, void* vehicle_ptr);

/**
 * Get the length of the track in segments.
 * 
 * @param sim_context Pointer to simulation context
 * @return Number of segments in the track
 */
RACEGYM_API int sim_get_track_length(void* sim_context);

/**
 * Check if the vehicle is off track.
 * 
 * @param sim_context Pointer to simulation context
 * @param vehicle_ptr Pointer to the vehicle
 * @return 1 if vehicle is off track, 0 otherwise
 */
RACEGYM_API int sim_is_vehicle_off_track(void* sim_context, void* vehicle_ptr);

/**
 * Get observation vector for the specified vehicle.
 * Observation layout: for each of 40 waypoints (20 pairs left/right) -> (local_x, local_z) relative to vehicle frame,
 * followed by longitudinal velocity, lateral velocity, and yaw rate. All values are in vehicle-local coordinates.
 *
 * @param sim_context Pointer to simulation context
 * @param vehicle_ptr Pointer to the vehicle
 * @param out_buffer Caller-allocated float buffer
 * @param max_floats Capacity of the buffer
 * @return Number of floats written
 */
RACEGYM_API int sim_get_observation(void* sim_context, void* vehicle_ptr, float* out_buffer, int max_floats);

/**
 * Get the vehicle's velocity vector in world coordinates.
 * 
 * @param vehicle_ptr Pointer to the vehicle
 * @param out_vel_xyz Output array [x, y, z] for velocity components
 */
RACEGYM_API void sim_get_vehicle_velocity(void* vehicle_ptr, float* out_vel_xyz);

/**
 * Get the track normal vector at a given track parameter.
 * 
 * @param sim_context Pointer to simulation context
 * @param t Track parameter position
 * @param out_normal_xy Output array [x, y] for normal vector components
 */
RACEGYM_API void sim_get_track_normal(void* sim_context, float t, float* out_normal_xy);

#ifdef __cplusplus
}
#endif

#endif // RACEGYM_SIM_H
