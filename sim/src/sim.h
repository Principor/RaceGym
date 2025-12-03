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

#ifdef __cplusplus
}
#endif

#endif // RACEGYM_SIM_H
