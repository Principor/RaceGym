#include "physics.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>


void PhysicsBody::applyForce(const glm::vec3 &force)
{
    accumulatedForce += force;
}

void PhysicsBody::applyForceAtPoint(const glm::vec3 &force, const glm::vec3 &point)
{
    accumulatedForce += force;
    glm::vec3 r = point - position;
    accumulatedTorque += glm::cross(r, force);
}

void PhysicsBody::step(float deltaTime)
{
    if(mass > 0.0f)
    {
        glm::vec3 acceleration = accumulatedForce / mass;
        velocity += acceleration * deltaTime;
        position += velocity * deltaTime;

        glm::vec3 angularAcceleration = accumulatedTorque / inertia;
        angularVelocity += angularAcceleration * deltaTime;
        
        // Integrate angular velocity into quaternion orientation
        glm::quat angularVelQuat(0.0f, angularVelocity.x, angularVelocity.y, angularVelocity.z);
        orientation += 0.5f * angularVelQuat * orientation * deltaTime;
        orientation = glm::normalize(orientation);
    }

    // Clear forces and torques for next step
    accumulatedForce = glm::vec3(0.0f);
    accumulatedTorque = glm::vec3(0.0f);
}

glm::mat4 PhysicsBody::getModelMatrix() const
{
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model *= glm::mat4_cast(orientation);
    return model;
}

void PhysicsWorld::stepSimulation(float deltaTime)
{
    for(auto body : bodies)
    {
        body->applyForce(gravity * body->mass);
        body->step(deltaTime);
    }
}

PhysicsBody* PhysicsWorld::addBody(CollisionShape const *shape, float mass, const glm::vec3 &position, const glm::quat &orientation)
{
    PhysicsBody *body = new PhysicsBody(shape, mass, position, orientation);
    bodies.push_back(body);
    return body;
    
}

void PhysicsWorld::removeBody(PhysicsBody* body)
{
    auto it = std::find(bodies.begin(), bodies.end(), body);
    if(it != bodies.end())
    {
        bodies.erase(it);
        delete body;
    }
}