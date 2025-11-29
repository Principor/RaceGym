#include "sim.h"
#include <chrono>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace {

struct SimContext {
    bool windowed;
    bool running;
    GLFWwindow* window;
    
    SimContext() : windowed(false), running(false), window(nullptr) {}
};

} // namespace

extern "C" {

RACEGYM_API void* sim_init(int windowed) {
    SimContext* ctx = new SimContext();
    ctx->windowed = (windowed != 0);
    ctx->running = true;
    
    if (ctx->windowed) {
        if (!glfwInit()) {
            delete ctx;
            return nullptr;
        }
        
        ctx->window = glfwCreateWindow(640, 480, "RaceGym Sim", nullptr, nullptr);
        if (!ctx->window) {
            glfwTerminate();
            delete ctx;
            return nullptr;
        }
        
        glfwMakeContextCurrent(ctx->window);
        glfwSwapInterval(1);
    }
    
    return ctx;
}

RACEGYM_API void sim_step(void* sim_context) {
    if (!sim_context) {
        return;
    }
    
    SimContext* ctx = static_cast<SimContext*>(sim_context);
    
    if (ctx->windowed && ctx->window && ctx->running) {
        if (glfwWindowShouldClose(ctx->window)) {
            ctx->running = false;
            return;
        }
        
        glfwPollEvents();
        // Minimal clear to black background
        // Use GLFW's default context; no GL calls needed for a blank window
        glfwSwapBuffers(ctx->window);
    }
    
    // Physics simulation and other updates would go here
}

RACEGYM_API void sim_shutdown(void* sim_context) {
    if (!sim_context) {
        return;
    }
    
    SimContext* ctx = static_cast<SimContext*>(sim_context);
    ctx->running = false;
    
    if (ctx->windowed && ctx->window) {
        glfwDestroyWindow(ctx->window);
        ctx->window = nullptr;
        glfwTerminate();
    }
    
    delete ctx;
}

}
