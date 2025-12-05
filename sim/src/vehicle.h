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

// Pacejka Magic Formula coefficients (simplified)
struct PacejkaCoefficients
{
    float B; // Stiffness factor
    float C; // Shape factor
    float D; // Peak factor
    float E; // Curvature factor
};

const PacejkaCoefficients PACEJKA_LONG = {10.0f, 1.9f, 1.0f, 0.97f};  // Longitudinal
const PacejkaCoefficients PACEJKA_LAT = {8.0f, 1.3f, 1.0f, -1.6f};    // Lateral

struct Wheel
{
    class Vehicle *vehicle;
    glm::vec3 localPosition; // Position relative to vehicle chassis
    float  restLength;    // Rest length of the suspension
    float wheelRadius;
    float suspensionStiffness;
    float suspensionDamping;
    float inertia;
    float compression; // Current compression of the suspension
    float angularVelocity; // Current wheel angular velocity
    float steerAngle;
    float driveTorque;
    float brakeTorque;
    glm::vec3 lastContactPoint; // Last point where wheel touched ground
    bool hasContact; // Whether wheel is currently in contact
    
    // Calculate Pacejka Magic Formula
    float calculatePacejka(float slip, const PacejkaCoefficients &coeff, float normalForce) const
    {
        float Fz = normalForce / 1000.0f; // Convert to kN for typical coefficients
        float D = coeff.D * Fz;
        float input = coeff.B * slip;
        float output = D * glm::sin(coeff.C * glm::atan(input - coeff.E * (input - glm::atan(input))));
        return output * 1000.0f; // Convert back to N
    }
};

class Vehicle
{
public:
    PhysicsBody *body;

    Vehicle(PhysicsWorld &world, const glm::vec3 &position, const glm::vec3 &rotation);
    ~Vehicle();

    void step(float deltaTime);
    void draw(int locModel, int locColor);

    void setSteerAmount(float steer); // -1.0 to 1.0
    void setThrottle(float throttle); // 0.0 to 1.0
    void setBrake(float brake);       // 0.0 to 1.0
    bool isOffTrack(class Track* track) const;

private:
    // Rendering data
    int numIndices;
    unsigned int vao, vbo, ebo;

    std::array<Wheel, 4> wheels;

    float steerAmount; // Current steering input
    float throttle;   // Current throttle
    float brake;      // Current brake
};

#endif // VEHICLE_H