#include "vehicle.h"
#include "track.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <algorithm>

Vehicle::Vehicle(PhysicsWorld &world, const glm::vec3 &position, const glm::vec3 &rotation)
{
    CollisionShape* boxShape = new BoxShape(VEHICLE_DIMENSIONS / 2.0f); // Example dimensions
    // Convert Euler angles to quaternion: rotation is assumed to be (pitch, yaw, roll)
    glm::quat orientation = glm::quat(glm::vec3(rotation.x, rotation.y, rotation.z));
    body = world.addBody(boxShape, VEHICLE_MASS, position, orientation);

    std::vector<glm::vec3> wheelPositions = {
        glm::vec3(+VEHICLE_DIMENSIONS.x * 0.5f, WHEEL_RADIUS - VEHICLE_DIMENSIONS.y * 0.5f, +VEHICLE_DIMENSIONS.z * 0.5f),  // Front-Right
        glm::vec3(-VEHICLE_DIMENSIONS.x * 0.5f, WHEEL_RADIUS - VEHICLE_DIMENSIONS.y * 0.5f, +VEHICLE_DIMENSIONS.z * 0.5f),  // Front-Left
        glm::vec3(+VEHICLE_DIMENSIONS.x * 0.5f, WHEEL_RADIUS - VEHICLE_DIMENSIONS.y * 0.5f, -VEHICLE_DIMENSIONS.z * 0.5f),  // Rear-Right
        glm::vec3(-VEHICLE_DIMENSIONS.x * 0.5f, WHEEL_RADIUS - VEHICLE_DIMENSIONS.y * 0.5f, -VEHICLE_DIMENSIONS.z * 0.5f)   // Rear-Left
    };
    for(int i = 0; i < 4; ++i)
    {
        wheels[i].vehicle = this;
        wheels[i].localPosition = wheelPositions[i];
        wheels[i].restLength = SUSPENSION_TRAVEL + WHEEL_RADIUS;
        wheels[i].wheelRadius = WHEEL_RADIUS;
        wheels[i].suspensionStiffness = SUSPENSION_STIFFNESS;
        wheels[i].suspensionDamping = SUSPENSION_DAMPING;
        wheels[i].inertia = 0.5f * 10.0f * WHEEL_RADIUS * WHEEL_RADIUS; // Assuming wheel mass of 10kg
        wheels[i].compression = 0.0f;
        wheels[i].angularVelocity = 0.0f;
        wheels[i].lastContactPoint = glm::vec3(0.0f);
        wheels[i].hasContact = false;
    }
    
    steerAmount = 0.0f;
    throttle = 0.0f;
    brake = 0.0f;

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

    wheels[0].steerAngle = steerAmount * glm::radians(30.0f); // Front-Right
    wheels[1].steerAngle = steerAmount * glm::radians(30.0f); // Front-Left
    wheels[2].steerAngle = 0.0f; // Rear-Right
    wheels[3].steerAngle = 0.0f; // Rear-Left

    float engineAngularVelocity = (wheels[2].angularVelocity + wheels[3].angularVelocity) / 2; // Simple average
    float enginePower = throttle * 110000.0f; // Max 110kW
    float engineTorque = glm::min(enginePower / glm::max(engineAngularVelocity, 1.0f), 3000.0f); // Limit max torque to 3000Nm
    float driveTorque = engineTorque * 0.5f; // Split torque to rear wheels
    wheels[0].driveTorque = 0.0f;
    wheels[1].driveTorque = 0.0f;
    wheels[2].driveTorque = driveTorque;
    wheels[3].driveTorque = driveTorque;

    float brakeTorque = brake * 2000.0f; // Max 2000Nm per wheel
    wheels[0].brakeTorque = brakeTorque;
    wheels[1].brakeTorque = brakeTorque;
    wheels[2].brakeTorque = brakeTorque;
    wheels[3].brakeTorque = brakeTorque;

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

        if (t < 0.0f) {
            wheels[i].hasContact = false;
            continue; // pointing away from ground
        }

        // Limit to suspension reach
        if (t > restLength) {
            wheels[i].hasContact = false;
            continue; // no contact within suspension
        }

        glm::vec3 contactPoint = mountWorld + suspAxisWorld * t;
        wheels[i].lastContactPoint = contactPoint;
        wheels[i].hasContact = true;

        // Compression is how much shorter than rest the ray is
        float compression = restLength - t;
        float compressionVelocity = (compression - wheels[i].compression) / deltaTime;
        float forceMag = k * compression + SUSPENSION_DAMPING * compressionVelocity;

        wheels[i].compression = compression;

        // Ground normal for y=0 plane
        glm::vec3 normal(0.0f, 1.0f, 0.0f);
        glm::vec3 suspensionForce = normal * forceMag;

        // Calculate tire forces using Pacejka Magic Formula
        // Get wheel world orientation (including steer angle)
        glm::quat steerQuat = glm::angleAxis(wheels[i].steerAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat wheelOrientation = body->orientation * steerQuat;
        
        // Forward and side directions in world space
        glm::vec3 forwardDir = wheelOrientation * glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 sideDir = wheelOrientation * glm::vec3(1.0f, 0.0f, 0.0f);
        
        // Velocity at contact point
        glm::vec3 r = contactPoint - body->position;
        glm::vec3 contactVelocity = body->velocity + glm::cross(body->angularVelocity, r);
        
        // Project velocity onto forward and side directions
        float forwardSpeed = glm::dot(contactVelocity, forwardDir);
        float sideSpeed = glm::dot(contactVelocity, sideDir);
        // Calculate slip ratio (longitudinal slip)
        float wheelCircumSpeed = wheels[i].angularVelocity * wheels[i].wheelRadius;
        float slipRatio = (wheelCircumSpeed - forwardSpeed) / std::max(glm::abs(forwardSpeed), 0.1f);
        // Calculate slip angle (lateral slip)
        float slipAngle  = glm::atan(-sideSpeed / std::max(glm::abs(forwardSpeed), 0.1f));
        
        // Apply Pacejka Magic Formula
        float normalForce = forceMag;
        float longitudinalForce = wheels[i].calculatePacejka(slipRatio, PACEJKA_LONG, normalForce);
        float lateralForce = wheels[i].calculatePacejka(slipAngle, PACEJKA_LAT, normalForce);
    
        // Combine forces in world space
        glm::vec3 tireForce = suspensionForce + forwardDir * longitudinalForce + sideDir * lateralForce;
        
        // Apply at contact point to induce correct torque
        body->applyForceAtPoint(tireForce, contactPoint);
        
        // Update wheel angular velocity
        // Torque on wheel = driveTorque - brakeTorque - longitudinalForce * wheelRadius
        float wheelTorque = wheels[i].driveTorque - wheels[i].brakeTorque - longitudinalForce * wheels[i].wheelRadius;
        float angularAcceleration = wheelTorque / wheels[i].inertia;
        wheels[i].angularVelocity += angularAcceleration * deltaTime;
    }

    body->applyForce(-body->velocity * glm::length(body->velocity) * 0.3f); // Simple drag
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

void Vehicle::setSteerAmount(float steer)
{
    this->steerAmount = std::clamp(steer, -1.0f, 1.0f);
}

void Vehicle::setThrottle(float throttleInput)
{
    this->throttle = std::clamp(throttleInput, 0.0f, 1.0f);
}

void Vehicle::setBrake(float brakeInput)
{
    this->brake = std::clamp(brakeInput, 0.0f, 1.0f);
}

bool Vehicle::isOffTrack(Track* track) const
{
    if (!track)
        return false;

    const float TRACK_WIDTH = 12.0f; // Must match the TRACK_WIDTH in track.cpp
    const float halfWidth = TRACK_WIDTH / 2.0f;

    // Check if any wheel is on track
    for (int i = 0; i < 4; ++i)
    {
        // Only consider wheels that have made contact at some point
        if (!wheels[i].hasContact && glm::length(wheels[i].lastContactPoint) < 0.01f)
            continue; // Wheel hasn't touched ground yet (start of episode)

        glm::vec3 contactPoint = wheels[i].lastContactPoint;
        glm::vec2 contactPoint2D(contactPoint.x, contactPoint.z);

        // Find closest point on track centerline
        float closestT = track->getClosestT(contactPoint2D);
        glm::vec2 trackPoint = track->getPosition(closestT);

        // Check distance from track centerline
        float distanceFromCenter = glm::distance(contactPoint2D, trackPoint);

        // If this wheel is within track bounds, vehicle is on track
        if (distanceFromCenter <= halfWidth)
            return false;
    }

    // All wheels are off track (or no wheels have contacted ground)
    return true;
}