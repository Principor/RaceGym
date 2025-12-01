#include "sim.h"
#include <chrono>

#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <string>
#include <iostream>

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

    // GL resources
    unsigned int vao;
    unsigned int vbo;
    unsigned int ebo;
    unsigned int shaderProgram;
    int locModel;
    int locView;
    int locProjection;
    int locColor;
    Camera camera;
    
    SimContext()
        : windowed(false), running(false), window(nullptr),
          vao(0), vbo(0), ebo(0), shaderProgram(0),
          locModel(-1), locView(-1), locProjection(-1), locColor(-1) {}
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

    glGenVertexArrays(1, &ctx->vao);
    glGenBuffers(1, &ctx->vbo);
    glGenBuffers(1, &ctx->ebo);

    glBindVertexArray(ctx->vao);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

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

    glBindVertexArray(ctx->vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

static void cleanupGraphics(SimContext* ctx) {
    if (ctx->ebo) { glDeleteBuffers(1, &ctx->ebo); ctx->ebo = 0; }
    if (ctx->vbo) { glDeleteBuffers(1, &ctx->vbo); ctx->vbo = 0; }
    if (ctx->vao) { glDeleteVertexArrays(1, &ctx->vao); ctx->vao = 0; }
    if (ctx->shaderProgram) { glDeleteProgram(ctx->shaderProgram); ctx->shaderProgram = 0; }
}

// Camera input helpers
static void processCameraInput(SimContext* ctx, float deltaTime) {
    Camera& cam = ctx->camera;
    GLFWwindow* window = ctx->window;
    glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), cam.direction));
    glm::vec3 up = glm::normalize(glm::cross(cam.direction, right));

    float velocity = cam.speed * deltaTime;
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
    
    delete ctx;
}

}
