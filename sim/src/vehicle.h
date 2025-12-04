#ifndef VEHICLE_H

#define VEHICLE_H

#include <array>
#include <glm/glm.hpp>
#include "physics.h"

const glm::vec3 VEHICLE_DIMENSIONS(2.0f, 1.0f, 4.0f); // Width, Height, Length in meters
const float VEHICLE_MASS = 1200.0f; // in kg

const float WHEEL_RADIUS = 0.35f; // in meters
const float SUSPENSION_TRAVEL = 0.5f; // in meters
const float SUSPENSION_STIFFNESS = 70000.0f; // N/m
const float SUSPENSION_DAMPING = 4500.0f; // Ns/m

struct Wheel
{
    class Vehicle *vehicle;
    glm::vec3 localPosition; // Position relative to vehicle chassis
    float  restLength;    // Rest length of the suspension
    float wheelRadius;
    float suspensionStiffness;
    float suspensionDamping;
    float compression; // Current compression of the suspension
};

class Vehicle
{
public:
    PhysicsBody *body;

    Vehicle(PhysicsWorld &world, const glm::vec3 &position, const glm::vec3 &rotation);
    ~Vehicle();

    void step(float deltaTime);
    void draw(int locModel, int locColor);

private:
    // Rendering data
    int numIndices;
    unsigned int vao, vbo, ebo;

    std::array<Wheel, 4> wheels;
};

#endif // VEHICLE_H