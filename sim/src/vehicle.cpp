#include "vehicle.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

Vehicle::Vehicle(PhysicsWorld &world, const glm::vec3 &position, const glm::vec3 &rotation)
{
    CollisionShape* boxShape = new BoxShape(VEHICLE_DIMENSIONS / 2.0f); // Example dimensions
    // Convert Euler angles to quaternion: rotation is assumed to be (pitch, yaw, roll)
    glm::quat orientation = glm::quat(glm::vec3(rotation.x, rotation.y, rotation.z));
    body = world.addBody(boxShape, VEHICLE_MASS, position, orientation);

    std::vector<glm::vec3> wheelPositions = {
        glm::vec3(+VEHICLE_DIMENSIONS.x * 0.5f, WHEEL_RADIUS - VEHICLE_DIMENSIONS.y * 0.5f, +VEHICLE_DIMENSIONS.z * 0.5f),
        glm::vec3(-VEHICLE_DIMENSIONS.x * 0.5f, WHEEL_RADIUS - VEHICLE_DIMENSIONS.y * 0.5f, +VEHICLE_DIMENSIONS.z * 0.5f),
        glm::vec3(+VEHICLE_DIMENSIONS.x * 0.5f, WHEEL_RADIUS - VEHICLE_DIMENSIONS.y * 0.5f, -VEHICLE_DIMENSIONS.z * 0.5f),
        glm::vec3(-VEHICLE_DIMENSIONS.x * 0.5f, WHEEL_RADIUS - VEHICLE_DIMENSIONS.y * 0.5f, -VEHICLE_DIMENSIONS.z * 0.5f)
    };
    for(int i = 0; i < 4; ++i)
    {
        wheels[i].vehicle = this;
        wheels[i].localPosition = wheelPositions[i];
        wheels[i].restLength = SUSPENSION_TRAVEL + WHEEL_RADIUS;
        wheels[i].wheelRadius = WHEEL_RADIUS;
        wheels[i].suspensionStiffness = SUSPENSION_STIFFNESS;
        wheels[i].suspensionDamping = SUSPENSION_DAMPING;
        wheels[i].compression = 0.0f;
    }
    

    // Create a simple box for rendering
    float w = VEHICLE_DIMENSIONS.x;
    float h = VEHICLE_DIMENSIONS.y;
    float l = VEHICLE_DIMENSIONS.z;
    float vertices[] = {
        -w/2, -h/2, -l/2,
         w/2, -h/2, -l/2,
         w/2,  h/2, -l/2,
        -w/2,  h/2, -l/2,
        -w/2, -h/2,  l/2,
         w/2, -h/2,  l/2,
         w/2,  h/2,  l/2,
        -w/2,  h/2,  l/2,
    };
    unsigned int indices[] = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        0, 1, 5, 5, 4, 0,
        2, 3, 7, 7, 6, 2,
        0, 3, 7, 7, 4, 0,
        1, 2, 6, 6, 5, 1,
    };
    numIndices = 36;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

Vehicle::~Vehicle()
{
    if (vao)
    {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    if (vbo)
    {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (ebo)
    {
        glDeleteBuffers(1, &ebo);
        ebo = 0;
    }
}

void Vehicle::step(float deltaTime)
{

    // Suspension axis is local -Y; rotate to world using quaternion
    glm::vec3 suspAxisWorld = body->orientation * glm::vec3(0.0f, -1.0f, 0.0f);

    // Simple spring-damper parameters
    float restLength = SUSPENSION_TRAVEL + WHEEL_RADIUS; // ray length from mount
    float k = SUSPENSION_STIFFNESS; // N/m spring stiffness
    for (int i = 0; i < 4; ++i)
    {
        // Wheel mount position in world using quaternion rotation
        glm::vec3 mountWorld = body->position + body->orientation * wheels[i].localPosition;

        // Raycast to infinite plane y=0 along suspAxisWorld
        // Solve mountWorld + suspAxisWorld * t => y = 0
        float denom = suspAxisWorld.y;
        if (glm::abs(denom) < 1e-4f)
            continue; // axis parallel to ground, skip
        float t = -mountWorld.y / denom;

        if (t < 0.0f)
            continue; // pointing away from ground

        // Limit to suspension reach
        if (t > restLength)
            continue; // no contact within suspension

        glm::vec3 contactPoint = mountWorld + suspAxisWorld * t;

        // Compression is how much shorter than rest the ray is
        float compression = restLength - t;
        float compressionVelocity = (compression - wheels[i].compression) / deltaTime;
        float forceMag = k * compression + SUSPENSION_DAMPING * compressionVelocity;

        wheels[i].compression = compression;

        // Ground normal for y=0 plane
        glm::vec3 normal(0.0f, 1.0f, 0.0f);
        glm::vec3 force = normal * forceMag;

        // Apply at contact point to induce correct torque
        body->applyForceAtPoint(force, contactPoint);
    }
}

void Vehicle::draw(int locModel, int locColor)
{
    glm::mat4 model = body->getModelMatrix();

    glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniform3f(locColor, 0.8f, 0.0f, 0.0f); // Red color for vehicle

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}