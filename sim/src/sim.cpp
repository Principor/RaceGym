#include "sim.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "track.h"
#include "physics.h"
#include "vehicle.h"
#include "renderer.h"

namespace {

struct SimContext {
    bool windowed;
    bool running;
    PhysicsWorld physicsWorld;
    Track* track;
    std::vector<Vehicle*> vehicles;

    SimContext() : windowed(false), running(false), track(nullptr) {}
};

}   // namespace

extern "C" {

RACEGYM_API void* sim_init(int windowed) {
    SimContext* ctx = new SimContext();
    ctx->windowed = (windowed != 0);
    ctx->running = true;

    if (ctx->windowed) {
        if (!Renderer::init()) {
            delete ctx;
            return nullptr;
        }
    }

    return ctx;
}

RACEGYM_API void sim_step(void* sim_context) {
    if (!sim_context) {
        return;
    }

    SimContext* ctx = static_cast<SimContext*>(sim_context);

    const float substepDelta = 1.0f / 100.0f;  // 0.01 seconds per physics step
    const int maxSubsteps = 10;

    auto startTime = std::chrono::high_resolution_clock::now();
    float simulatedTime = 0.0f;
    int substepsCompleted = 0;
    bool hasRendered = false;

    bool hasWindow = ctx->windowed && Renderer::is_initialized();

    while (substepsCompleted < maxSubsteps) {
        // Check if we should renderCtx or simulate
        bool shouldRender = false;
        if (hasWindow && ctx->running) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = currentTime - startTime;
            float elapsedSeconds = elapsed.count();

            // Render if we're ahead of real-time or if this is the last physics step
            shouldRender = (simulatedTime >= elapsedSeconds) || (substepsCompleted == maxSubsteps);
        }

        if (shouldRender) {
            Renderer::render_step(ctx->track, ctx->vehicles, ctx->running);
            if (!ctx->running) return;
            hasRendered = true;
        } else {
            // Perform physics step
            ctx->physicsWorld.stepSimulation(substepDelta);
            for (auto vehicle : ctx->vehicles) {
                vehicle->step(substepDelta);
            }
            substepsCompleted++;
            simulatedTime += substepDelta;
        }
    }

    // Ensure at least one renderer if windowed and we somehow didn't render yet
    if (hasWindow && ctx->running && !hasRendered) {
        Renderer::render_step(ctx->track, ctx->vehicles, ctx->running);
    }
}

RACEGYM_API void sim_shutdown(void* sim_context) {
    if (!sim_context) {
        return;
    }

    SimContext* ctx = static_cast<SimContext*>(sim_context);
    ctx->running = false;

    if (ctx->windowed) {
        Renderer::shutdown();
    }

    if (ctx->track) {
        delete ctx->track;
        ctx->track = nullptr;
    }

    delete ctx;
}

RACEGYM_API void sim_load_track(void* sim_context, const char* path) {
    if (!sim_context || !path) {
        return;
    }

    SimContext* ctx = static_cast<SimContext*>(sim_context);

    if(ctx->track) {
        delete ctx->track;
        for(auto vehicle : ctx->vehicles) {
            delete vehicle;
        }
        ctx->vehicles.clear(); // Clear physics bodies to prevent dangling pointers
    }

    ctx->track = new Track(path);
}

RACEGYM_API void* sim_add_vehicle(void* sim_context, float spawnT) {
    if (!sim_context) {
        return nullptr;
    }

    SimContext* ctx = static_cast<SimContext*>(sim_context);

    if(!ctx->track) {
        std::cerr << "Cannot add vehicle: no track loaded." << std::endl;
        return nullptr;
    }

    glm::vec2 startPos = ctx->track->getPosition(spawnT);
    glm::vec2 startTangent = ctx->track->getTangent(spawnT);
    float startAngle = atan2(startTangent.x, startTangent.y);    

    Vehicle *vehicle = new Vehicle(ctx->physicsWorld, glm::vec3(startPos.x, 0.75f, startPos.y), glm::vec3(0.0f, startAngle, 0.0f));
    ctx->vehicles.push_back(vehicle);
    return vehicle;
}

RACEGYM_API void sim_remove_vehicle(void* sim_context, void* vehicle_ptr) {
    if (!sim_context || !vehicle_ptr) {
        return;
    }

    SimContext* ctx = static_cast<SimContext*>(sim_context);
    Vehicle* vehicle = static_cast<Vehicle*>(vehicle_ptr);

    auto it = std::find(ctx->vehicles.begin(), ctx->vehicles.end(), vehicle);
    if (it != ctx->vehicles.end()) {
        ctx->vehicles.erase(it);
        delete vehicle;
    }
}

RACEGYM_API void sim_set_vehicle_control(void* vehicle_ptr, float steer, float throttle, float brake) {
    if (!vehicle_ptr) {
        return;
    }

    Vehicle* vehicle = static_cast<Vehicle*>(vehicle_ptr);
    vehicle->setSteerAmount(steer);   // -1.0 to 1.0
    vehicle->setThrottle(throttle);   // 0.0 to 1.0
    vehicle->setBrake(brake);         // 0.0 to 1.0
}

RACEGYM_API float sim_get_vehicle_track_position(void* sim_context, void* vehicle_ptr) {
    if (!sim_context || !vehicle_ptr) {
        return 0.0f;
    }

    SimContext* ctx = static_cast<SimContext*>(sim_context);
    if (!ctx->track) {
        return 0.0f;
    }

    Vehicle* vehicle = static_cast<Vehicle*>(vehicle_ptr);
    glm::vec3 vehiclePos = vehicle->body->position;
    glm::vec2 vehiclePos2D(vehiclePos.x, vehiclePos.z);

    return ctx->track->getClosestT(vehiclePos2D);
}

RACEGYM_API int sim_get_track_length(void* sim_context) {
    if (!sim_context) {
        return 0;
    }

    SimContext* ctx = static_cast<SimContext*>(sim_context);
    if (!ctx->track) {
        return 0;
    }

    return ctx->track->getNumSegments();
}

RACEGYM_API int sim_is_vehicle_off_track(void* sim_context, void* vehicle_ptr) {
    if (!sim_context || !vehicle_ptr) {
        return 0;
    }

    SimContext* ctx = static_cast<SimContext*>(sim_context);
    Vehicle* vehicle = static_cast<Vehicle*>(vehicle_ptr);

    return vehicle->isOffTrack(ctx->track) ? 1 : 0;
}

RACEGYM_API int sim_get_observation(void* sim_context, void* vehicle_ptr, float* out_buffer, int max_floats) {
    if (!sim_context || !vehicle_ptr || !out_buffer || max_floats <= 0) {
        return 0;
    }

    SimContext* ctx = static_cast<SimContext*>(sim_context);
    Vehicle* vehicle = static_cast<Vehicle*>(vehicle_ptr);
    if (!ctx->track) {
        return 0;
    }

    // Current track parameter
    glm::vec3 vehiclePos = vehicle->body->position;
    float currentT = ctx->track->getClosestT(glm::vec2(vehiclePos.x, vehiclePos.z));

    // Generate waypoints (20 pairs => 40 points)
    const int numWaypoints = 20;
    const float waypointSpacing = 0.1f;
    std::vector<glm::vec3> waypoints = ctx->track->getWaypoints(currentT, numWaypoints, waypointSpacing);

    // Vehicle frame
    glm::vec3 forward = glm::normalize(vehicle->body->orientation * glm::vec3(0.0f, 0.0f, 1.0f));
    glm::vec3 right   = glm::normalize(vehicle->body->orientation * glm::vec3(1.0f, 0.0f, 0.0f));

    int idx = 0;
    for (const auto& wp : waypoints) {
        if (idx + 2 > max_floats) break;
        glm::vec3 rel = wp - vehiclePos;
        out_buffer[idx++] = glm::dot(rel, right);    // local x (lateral)
        out_buffer[idx++] = glm::dot(rel, forward);  // local z (longitudinal)
    }

    if (idx + 3 <= max_floats) {
        glm::vec3 vel = vehicle->body->velocity;
        out_buffer[idx++] = glm::dot(vel, forward); // longitudinal velocity
        out_buffer[idx++] = glm::dot(vel, right);   // lateral velocity
        out_buffer[idx++] = vehicle->body->angularVelocity.y; // yaw rate
    }

    return idx;
}

RACEGYM_API void sim_get_vehicle_velocity(void* vehicle_ptr, float* out_vel_xyz) {
    if (!vehicle_ptr || !out_vel_xyz) {
        return;
    }

    Vehicle* vehicle = static_cast<Vehicle*>(vehicle_ptr);
    glm::vec3 vel = vehicle->body->velocity;
    out_vel_xyz[0] = vel.x;
    out_vel_xyz[1] = vel.y;
    out_vel_xyz[2] = vel.z;
}

RACEGYM_API void sim_get_track_normal(void* sim_context, float t, float* out_normal_xy) {
    if (!sim_context || !out_normal_xy) {
        return;
    }

    SimContext* ctx = static_cast<SimContext*>(sim_context);
    if (!ctx->track) {
        out_normal_xy[0] = 0.0f;
        out_normal_xy[1] = 0.0f;
        return;
    }

    glm::vec2 normal = ctx->track->getNormal(t);
    out_normal_xy[0] = normal.x;
    out_normal_xy[1] = normal.y;
}

RACEGYM_API int sim_is_vehicle_crashed(void* sim_context, void* vehicle_ptr) {
    if (!sim_context || !vehicle_ptr) {
        return 0;
    }

    SimContext* ctx = static_cast<SimContext*>(sim_context);
    Vehicle* vehicle = static_cast<Vehicle*>(vehicle_ptr);
    
    glm::vec3 pos = vehicle->body->position;
    glm::quat orientation = vehicle->body->orientation;
    
    // Check if underground (y < -2.0)
    if (pos.y < -2.0f) {
        return 1;
    }
    
    // Check if too high in the air (y > 20.0)
    if (pos.y > 20.0f) {
        return 1;
    }
    
    // Check if upside down - get the up vector in world space
    glm::vec3 up = orientation * glm::vec3(0.0f, 1.0f, 0.0f);
    // If the dot product with world up is negative, vehicle is upside down
    if (up.y < -0.1f) {
        return 1;
    }
    
    // Check if too far from track (horizontal distance > 100 units)
    if (ctx->track) {
        glm::vec2 vehiclePos2D = glm::vec2(pos.x, pos.z);
        float t = ctx->track->getClosestT(vehiclePos2D);
        glm::vec2 trackPos = ctx->track->getPosition(t);
        float distanceFromTrack = glm::distance(vehiclePos2D, trackPos);
        if (distanceFromTrack > 100.0f) {
            return 1;
        }
    }
    
    return 0;
}

} // extern "C"