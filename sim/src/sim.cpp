#include "sim.h"
#include <chrono>

#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <string>
#include <vector>
#include <iostream>
#include "track.h"
#include "physics.h"
#include "vehicle.h"

namespace {

struct Camera {
        glm::vec3 position;
        float yaw;   // left/right
        float pitch; // up/down
        float speed;
        float sensitivity;
        bool rightMouseDown;
        float lastX, lastY;
        bool firstMouse;
        glm::vec3 direction;
        Camera()
                : position(0.0f, 30.0f, 0.0f), yaw(0.0f), pitch(0.0f), speed(20.0f), sensitivity(0.1f),
                    rightMouseDown(false), lastX(0.0f), lastY(0.0f), firstMouse(true), direction(0,0,1) {}
};

struct SimContext {
    bool windowed;
    bool running;
    GLFWwindow* window;

    PhysicsWorld physicsWorld;

    Track *track;

    std::vector<Vehicle*> vehicles;

    // GL resources
    unsigned int groundPlaneVao;
    unsigned int groundPlaneVbo;
    unsigned int groundPlaneEbo;
    unsigned int shaderProgram;
    int locModel;
    int locView;
    int locProjection;
    int locColor;
    Camera camera;
    
    // Waypoint rendering
    unsigned int waypointVao, waypointVbo, waypointEbo;
    
    SimContext()
        : windowed(false), running(false), window(nullptr), track(nullptr),
          groundPlaneVao(0), groundPlaneVbo(0), groundPlaneEbo(0), shaderProgram(0),
          locModel(-1), locView(-1), locProjection(-1), locColor(-1),
          waypointVao(0), waypointVbo(0), waypointEbo(0)
          {}
                // Camera is default-initialized
};

static const char* kVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
void main(){
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
)GLSL";

static const char* kFragmentShader = R"GLSL(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
void main(){
    FragColor = vec4(uColor, 1.0);
}
)GLSL";

static void logShaderError(unsigned int shader) {
    int len = 0; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    if (len > 1) {
        std::string log; log.resize(len);
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        std::cerr << "Shader compile error: " << log << std::endl;
    }
}

static void logProgramError(unsigned int prog) {
    int len = 0; glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
    if (len > 1) {
        std::string log; log.resize(len);
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::cerr << "Program link error: " << log << std::endl;
    }
}

static unsigned int compileShader(unsigned int type, const char* src) {
    unsigned int s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { logShaderError(s); }
    return s;
}

static unsigned int linkProgram(unsigned int vs, unsigned int fs) {
    unsigned int p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    int ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { logProgramError(p); }
    glDetachShader(p, vs);
    glDetachShader(p, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

static bool initGraphics(SimContext* ctx) {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    // Keep culling disabled for now so the quad shows regardless of winding
    glDisable(GL_CULL_FACE);

    unsigned int vs = compileShader(GL_VERTEX_SHADER, kVertexShader);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    ctx->shaderProgram = linkProgram(vs, fs);
    ctx->locModel = glGetUniformLocation(ctx->shaderProgram, "uModel");
    ctx->locView = glGetUniformLocation(ctx->shaderProgram, "uView");
    ctx->locProjection = glGetUniformLocation(ctx->shaderProgram, "uProjection");
    ctx->locColor = glGetUniformLocation(ctx->shaderProgram, "uColor");

    // Simple ground plane
    const float planeSize = 1000.0f;
    const float vertices[] = {
             0.0f, 0.0f,      0.0f,
        planeSize, 0.0f,      0.0f,
        planeSize, 0.0f, planeSize,
             0.0f, 0.0f, planeSize,
    };
    const unsigned int indices[] = { 0, 1, 2, 0, 2, 3 };

    glGenVertexArrays(1, &ctx->groundPlaneVao);
    glGenBuffers(1, &ctx->groundPlaneVbo);
    glGenBuffers(1, &ctx->groundPlaneEbo);

    glBindVertexArray(ctx->groundPlaneVao);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->groundPlaneVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->groundPlaneEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);
    
    // Create waypoint cube mesh
    glGenVertexArrays(1, &ctx->waypointVao);
    glGenBuffers(1, &ctx->waypointVbo);
    glGenBuffers(1, &ctx->waypointEbo);
    
    glBindVertexArray(ctx->waypointVao);
    
    // Unit cube vertices
    const float cubeVertices[] = {
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
    };
    
    const unsigned int cubeIndices[] = {
        0, 1, 2, 2, 3, 0,  // back
        4, 5, 6, 6, 7, 4,  // front
        0, 1, 5, 5, 4, 0,  // bottom
        2, 3, 7, 7, 6, 2,  // top
        0, 3, 7, 7, 4, 0,  // left
        1, 2, 6, 6, 5, 1,  // right
    };
    
    glBindBuffer(GL_ARRAY_BUFFER, ctx->waypointVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->waypointEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    
    glBindVertexArray(0);
    
    return true;
}

static void renderScene(SimContext* ctx) {
    int display_w = 0, display_h = 0;
    glfwGetFramebufferSize(ctx->window, &display_w, &display_h);
    if (display_w <= 0 || display_h <= 0) return;
    glViewport(0, 0, display_w, display_h);

    // Sky: night light blue
    glClearColor(135.0f/255.0f, 206.0f/255.0f, 235.0f/255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Camera & matrices
    const float aspect = static_cast<float>(display_w) / static_cast<float>(display_h);
    glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 1000.0f);
    projection[0][0] *= -1; // Invert X for GLM's RH coordinate system
    Camera& cam = ctx->camera;
    glm::mat4 view = glm::lookAt(cam.position, cam.position + cam.direction, glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 model(1.0f);  // Identity for ground plane at origin

    glUseProgram(ctx->shaderProgram);
    glUniformMatrix4fv(ctx->locProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(ctx->locView, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(ctx->locModel, 1, GL_FALSE, glm::value_ptr(model));
    // Light green grass color
    glUniform3f(ctx->locColor, 144.0f/255.0f, 238.0f/255.0f, 144.0f/255.0f);

    // Push ground plane back in depth so everything else renders on top
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);

    glBindVertexArray(ctx->groundPlaneVao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glDisable(GL_POLYGON_OFFSET_FILL);
    
    if (ctx->track) {
        ctx->track->draw(ctx->locModel, ctx->locColor);
    }

    for (auto vehicle : ctx->vehicles) {
        vehicle->draw(ctx->locModel, ctx->locColor);
    }
    
    // Draw waypoints
    if (!ctx->vehicles.empty()) {
        glUseProgram(ctx->shaderProgram);
        glUniform3f(ctx->locColor, 1.0f, 1.0f, 0.0f); // Yellow color
        
        glBindVertexArray(ctx->waypointVao);
        
        const float size = 0.3f;

        Vehicle *vehicle = ctx->vehicles[0];
        glm::vec2 vehiclePos2D = glm::vec2(vehicle->body->position.x, vehicle->body->position.z);
        const float currentT = ctx->track->getClosestT(vehiclePos2D);

        for (const auto& waypoint : ctx->track->getWaypoints(currentT, 20, 0.1f)) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), waypoint);
            model = glm::scale(model, glm::vec3(size));
            glUniformMatrix4fv(ctx->locModel, 1, GL_FALSE, glm::value_ptr(model));
            
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }
        
        glBindVertexArray(0);
    }
}

static void cleanupGraphics(SimContext* ctx) {
    if (ctx->waypointEbo) { glDeleteBuffers(1, &ctx->waypointEbo); ctx->waypointEbo = 0; }
    if (ctx->waypointVbo) { glDeleteBuffers(1, &ctx->waypointVbo); ctx->waypointVbo = 0; }
    if (ctx->waypointVao) { glDeleteVertexArrays(1, &ctx->waypointVao); ctx->waypointVao = 0; }
    if (ctx->groundPlaneEbo) { glDeleteBuffers(1, &ctx->groundPlaneEbo); ctx->groundPlaneEbo = 0; }
    if (ctx->groundPlaneVbo) { glDeleteBuffers(1, &ctx->groundPlaneVbo); ctx->groundPlaneVbo = 0; }
    if (ctx->groundPlaneVao) { glDeleteVertexArrays(1, &ctx->groundPlaneVao); ctx->groundPlaneVao = 0; }
    if (ctx->shaderProgram) { glDeleteProgram(ctx->shaderProgram); ctx->shaderProgram = 0; }
}

// Camera input helpers
static void processCameraInput(SimContext* ctx, float deltaTime) {
    Camera& cam = ctx->camera;
    GLFWwindow* window = ctx->window;
    glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), cam.direction));
    glm::vec3 up = glm::normalize(glm::cross(cam.direction, right));

    float velocity = cam.speed * deltaTime * (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 5.0f : 1.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cam.position += cam.direction * velocity;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cam.position -= cam.direction * velocity;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cam.position -= right * velocity;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cam.position += right * velocity;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        cam.position += up * velocity;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        cam.position -= up * velocity;
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    SimContext* ctx = static_cast<SimContext*>(glfwGetWindowUserPointer(window));
    if (!ctx) return;
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            ctx->camera.rightMouseDown = true;
            ctx->camera.firstMouse = true;
        } else if (action == GLFW_RELEASE) {
            ctx->camera.rightMouseDown = false;
        }
    }
}

static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    SimContext* ctx = static_cast<SimContext*>(glfwGetWindowUserPointer(window));
    if (!ctx) return;
    Camera& cam = ctx->camera;
    if (!cam.rightMouseDown) return;
    if (cam.firstMouse) {
        cam.lastX = (float)xpos;
        cam.lastY = (float)ypos;
        cam.firstMouse = false;
    }
    float xoffset = (float)xpos - cam.lastX;
    float yoffset = cam.lastY - (float)ypos; // reversed: y ranges bottom to top
    cam.lastX = (float)xpos;
    cam.lastY = (float)ypos;
    xoffset *= cam.sensitivity;
    yoffset *= cam.sensitivity;
    cam.yaw -= xoffset;
    cam.pitch -= yoffset;
    if (cam.pitch > 89.0f) cam.pitch = 89.0f;
    if (cam.pitch < -89.0f) cam.pitch = -89.0f;
    cam.direction.x = sin(glm::radians(cam.yaw)) * cos(glm::radians(cam.pitch));
    cam.direction.y = sin(glm::radians(cam.pitch));
    cam.direction.z = cos(glm::radians(cam.yaw)) * cos(glm::radians(cam.pitch));
}

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
        
        // Request a modern OpenGL context (3.3 core)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        ctx->window = glfwCreateWindow(640, 480, "RaceGym Sim", nullptr, nullptr);
        if (!ctx->window) {
            glfwTerminate();
            delete ctx;
            return nullptr;
        }
        
        glfwMakeContextCurrent(ctx->window);
        glfwSwapInterval(1);
        
        // Set up input callbacks for camera
        glfwSetWindowUserPointer(ctx->window, ctx);
        glfwSetMouseButtonCallback(ctx->window, mouseButtonCallback);
        glfwSetCursorPosCallback(ctx->window, cursorPosCallback);
        // Initialize GL loader and resources
        if (!initGraphics(ctx)) {
            glfwDestroyWindow(ctx->window);
            glfwTerminate();
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

    for(int i = 0; i < 10; i++)
    {
        const float substepDelta = 1.0f / 600.0f; // 10 substeps for 60Hz
        ctx->physicsWorld.stepSimulation(substepDelta); // Fixed timestep for now
        for (auto vehicle : ctx->vehicles) {
            vehicle->step(substepDelta);
        }
    }

    if (ctx->windowed && ctx->window && ctx->running) {
        if (glfwWindowShouldClose(ctx->window)) {
            ctx->running = false;
            return;
        }
        
        glfwPollEvents();
        // Camera movement: estimate deltaTime for smooth movement
        static double lastTime = glfwGetTime();
        double now = glfwGetTime();
        float deltaTime = static_cast<float>(now - lastTime);
        lastTime = now;
        processCameraInput(ctx, deltaTime);
        // Render the scene
        renderScene(ctx);
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
        // Clean up GL resources before destroying context
        glfwMakeContextCurrent(ctx->window);
        cleanupGraphics(ctx);
        glfwDestroyWindow(ctx->window);
        ctx->window = nullptr;
        glfwTerminate();
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
    const float trackWidth = 12.0f;
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

} // extern "C"