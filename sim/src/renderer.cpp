#include "renderer.h"

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
#include "vehicle.h"

namespace {

struct Camera {
    glm::vec3 position;
    float yaw;
    float pitch;
    float speed;
    float sensitivity;
    bool rightMouseDown;
    float lastX;
    float lastY;
    bool firstMouse;
    glm::vec3 direction;
    Camera();
};

struct RenderContext {
    GLFWwindow* window;
    unsigned int shaderProgram;
    int locModel;
    int locView;
    int locProjection;
    int locColor;
    Camera camera;
    double lastCameraTime;
    Mesh groundPlaneMesh, waypointMesh;
    RenderContext();
};

static RenderContext g_ctx;
static bool g_initialized = false;

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

static bool initGraphics(RenderContext* ctx) {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    unsigned int vs = compileShader(GL_VERTEX_SHADER, kVertexShader);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    ctx->shaderProgram = linkProgram(vs, fs);
    ctx->locModel = glGetUniformLocation(ctx->shaderProgram, "uModel");
    ctx->locView = glGetUniformLocation(ctx->shaderProgram, "uView");
    ctx->locProjection = glGetUniformLocation(ctx->shaderProgram, "uProjection");
    ctx->locColor = glGetUniformLocation(ctx->shaderProgram, "uColor");

    const float planeSize = 1000.0f;
    const float vertices[] = {
             0.0f, 0.0f,      0.0f,
        planeSize, 0.0f,      0.0f,
        planeSize, 0.0f, planeSize,
             0.0f, 0.0f, planeSize,
    };
    const unsigned int indices[] = { 0, 1, 2, 0, 2, 3 };
    ctx->groundPlaneMesh = Renderer::createMesh(vertices, 4, indices, 6);

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
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        0, 1, 5, 5, 4, 0,
        2, 3, 7, 7, 6, 2,
        0, 3, 7, 7, 4, 0,
        1, 2, 6, 6, 5, 1,
    };
    ctx->waypointMesh = Renderer::createMesh(cubeVertices, 8, cubeIndices, 36);

    return true;
}

static void renderScene(RenderContext* ctx, Track* track, const std::vector<Vehicle*>& vehicles) {
    int display_w = 0, display_h = 0;
    glfwGetFramebufferSize(ctx->window, &display_w, &display_h);
    if (display_w <= 0 || display_h <= 0) return;
    glViewport(0, 0, display_w, display_h);

    glClearColor(135.0f/255.0f, 206.0f/255.0f, 235.0f/255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float aspect = static_cast<float>(display_w) / static_cast<float>(display_h);
    glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 1000.0f);
    projection[0][0] *= -1;
    Camera& cam = ctx->camera;
    glm::mat4 view = glm::lookAt(cam.position, cam.position + cam.direction, glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 model(1.0f);

    glUseProgram(ctx->shaderProgram);
    glUniformMatrix4fv(ctx->locProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(ctx->locView, 1, GL_FALSE, glm::value_ptr(view));

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    Renderer::drawMesh(ctx->groundPlaneMesh, glm::mat4(1.0f), glm::vec3(144.0f/255.0f, 238.0f/255.0f, 144.0f/255.0f));
    glDisable(GL_POLYGON_OFFSET_FILL);

    if (track) {
        track->draw(ctx->locModel, ctx->locColor);
    }

    for (auto vehicle : vehicles) {
        vehicle->draw(ctx->locModel, ctx->locColor);
    }

    if (track && !vehicles.empty()) {
        const float size = 0.3f;

        Vehicle* vehicle = vehicles[0];
        glm::vec2 vehiclePos2D = glm::vec2(vehicle->body->position.x, vehicle->body->position.z);
        const float currentT = track->getClosestT(vehiclePos2D);

        for (const auto& waypoint : track->getWaypoints(currentT, 20, 0.1f)) {
            glm::mat4 waypointModel = glm::translate(glm::mat4(1.0f), waypoint);
            waypointModel = glm::scale(waypointModel, glm::vec3(size));
            Renderer::drawMesh(ctx->waypointMesh, waypointModel, glm::vec3(1.0f, 1.0f, 0.0f));
        }
    }
}

static void cleanupGraphics(RenderContext* ctx) {
    Renderer::destroyMesh(ctx->groundPlaneMesh);
    Renderer::destroyMesh(ctx->waypointMesh);
    if (ctx->shaderProgram) { glDeleteProgram(ctx->shaderProgram); ctx->shaderProgram = 0; }
}

static void processCameraInput(RenderContext* ctx, float deltaTime) {
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
    RenderContext* ctx = static_cast<RenderContext*>(glfwGetWindowUserPointer(window));
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
    RenderContext* ctx = static_cast<RenderContext*>(glfwGetWindowUserPointer(window));
    if (!ctx) return;
    Camera& cam = ctx->camera;
    if (!cam.rightMouseDown) return;
    if (cam.firstMouse) {
        cam.lastX = static_cast<float>(xpos);
        cam.lastY = static_cast<float>(ypos);
        cam.firstMouse = false;
    }
    float xoffset = static_cast<float>(xpos) - cam.lastX;
    float yoffset = cam.lastY - static_cast<float>(ypos);
    cam.lastX = static_cast<float>(xpos);
    cam.lastY = static_cast<float>(ypos);
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

Camera::Camera()
    : position(0.0f, 30.0f, 0.0f),
      yaw(0.0f),
      pitch(0.0f),
      speed(20.0f),
      sensitivity(0.1f),
      rightMouseDown(false),
      lastX(0.0f),
      lastY(0.0f),
      firstMouse(true),
      direction(0.0f, 0.0f, 1.0f) {}

RenderContext::RenderContext()
        : window(nullptr),
      shaderProgram(0),
      locModel(-1),
      locView(-1),
      locProjection(-1),
      locColor(-1),
      lastCameraTime(0.0) {}


bool Renderer::init() {
    if (g_initialized) {
        std::cerr << "Renderer already initialized." << std::endl;
        return false;
    }

    g_ctx = RenderContext();
    g_ctx.lastCameraTime = 0.0;

    if (!glfwInit()) {
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    g_ctx.window = glfwCreateWindow(640, 480, "RaceGym Sim", nullptr, nullptr);
    if (!g_ctx.window) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(g_ctx.window);
    glfwSwapInterval(1);

    glfwSetWindowUserPointer(g_ctx.window, &g_ctx);
    glfwSetMouseButtonCallback(g_ctx.window, mouseButtonCallback);
    glfwSetCursorPosCallback(g_ctx.window, cursorPosCallback);

    if (!initGraphics(&g_ctx)) {
        glfwDestroyWindow(g_ctx.window);
        g_ctx.window = nullptr;
        glfwTerminate();
        return false;
    }

    g_initialized = true;
    return true;
}

bool Renderer::is_initialized() {
    return g_initialized;
}

void Renderer::shutdown() {
    if (!g_initialized) {
        return;
    }

    if (g_ctx.window) {
        glfwMakeContextCurrent(g_ctx.window);
        cleanupGraphics(&g_ctx);
        glfwDestroyWindow(g_ctx.window);
        g_ctx.window = nullptr;
        glfwTerminate();
    }

    g_initialized = false;
}

void Renderer::render_step(Track* track, const std::vector<Vehicle*>& vehicles, bool& running) {
    if (!g_initialized || !g_ctx.window) {
        return;
    }

    if (glfwWindowShouldClose(g_ctx.window)) {
        running = false;
        return;
    }

    glfwPollEvents();

    double now = glfwGetTime();
    float deltaTime = g_ctx.lastCameraTime == 0.0 ? 0.0f : static_cast<float>(now - g_ctx.lastCameraTime);
    g_ctx.lastCameraTime = now;
    processCameraInput(&g_ctx, deltaTime);

    renderScene(&g_ctx, track, vehicles);
    glfwSwapBuffers(g_ctx.window);
}

Mesh Renderer::createMesh(const float* vertices, int numVertices, const unsigned int* indices, int numIndices) {
    Mesh mesh;
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);
    mesh.numIndices = numIndices;

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, numVertices * 3 * sizeof(float), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * sizeof(unsigned int), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);

    return mesh;
}

void Renderer::drawMesh(const Mesh& mesh, glm::mat4 modelMatrix, glm::vec3 colour, int drawMode) {
    glUseProgram(g_ctx.shaderProgram);
    glUniformMatrix4fv(g_ctx.locModel, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniform3f(g_ctx.locColor, colour.r, colour.g, colour.b);

    glBindVertexArray(mesh.vao);
    glDrawElements(drawMode, mesh.numIndices, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Renderer::destroyMesh(Mesh& mesh) {
    if (mesh.ebo) { glDeleteBuffers(1, &mesh.ebo); mesh.ebo = 0; }
    if (mesh.vbo) { glDeleteBuffers(1, &mesh.vbo); mesh.vbo = 0; }
    if (mesh.vao) { glDeleteVertexArrays(1, &mesh.vao); mesh.vao = 0; }
    mesh.numIndices = 0;
}