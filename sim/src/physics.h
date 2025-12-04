#ifndef PHYSICS_H

#define PHYSICS_H

#include <glm/glm.hpp>
#include <vector>
#include <glm/gtc/quaternion.hpp>

enum CollisionShapeType
{
    SHAPE_TYPE_BOX,
};

class CollisionShape
{
public:
    const CollisionShapeType type;
    CollisionShape(CollisionShapeType type) : type(type) {}
    virtual glm::vec3 getInertiaTensor(float mass) const = 0;
};

class BoxShape : public CollisionShape
{
public:
    glm::vec3 halfExtents;
    BoxShape(const glm::vec3 &halfExtents)
        : CollisionShape(SHAPE_TYPE_BOX), halfExtents(halfExtents) {}
    glm::vec3 getInertiaTensor(float mass) const override{
        return (1.0f / 12.0f) * mass * glm::vec3(
            halfExtents.y * 2 * halfExtents.y * 2 + halfExtents.z * 2 * halfExtents.z * 2,
            halfExtents.x * 2 * halfExtents.x * 2 + halfExtents.z * 2 * halfExtents.z * 2,
            halfExtents.x * 2 * halfExtents.x * 2 + halfExtents.y * 2 * halfExtents.y * 2
        );
    }
};

struct PhysicsBody
{
public:
    float mass;
    glm::vec3 inertia;
    glm::vec3 position;
    glm::vec3 velocity;
    glm::quat orientation;
    glm::vec3 angularVelocity;
    CollisionShape const *shape;

    PhysicsBody(CollisionShape const *shape, float mass, const glm::vec3 &position, const glm::quat &orientation)
        : shape(shape), mass(mass), position(position), velocity(0.0f),
          orientation(orientation), angularVelocity(0.0f),
          accumulatedForce(0.0f), accumulatedTorque(0.0f)
    {
        if(mass > 0.0f)
            inertia = shape->getInertiaTensor(mass);
    }

    ~PhysicsBody()
    {
        delete shape;
    }

    void applyForce(const glm::vec3 &force);
    void applyForceAtPoint(const glm::vec3 &force, const glm::vec3 &point);
    void step(float deltaTime);
    glm::mat4 getModelMatrix() const;

private:
    glm::vec3 accumulatedForce;
    glm::vec3 accumulatedTorque;
};

struct ContactPoint
{
    glm::vec3 posA;
    glm::vec3 posB;
    glm::vec3 normal;
    float penetrationDepth;
};

struct CollisionInfo
{
    PhysicsBody *bodyA;
    PhysicsBody *bodyB;
    std::vector<ContactPoint> contactPoints;
};

class PhysicsWorld
{
public:
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);

    void stepSimulation(float deltaTime);

    PhysicsBody* addBody(CollisionShape const *shape, float mass=0.0f, const glm::vec3 &position=glm::vec3(0.0f), const glm::quat &orientation=glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

private:
    std::vector<PhysicsBody> bodies;
};

#endif // PHYSICS_H