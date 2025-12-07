#ifndef TRACK_H

#define TRACK_H

#include <vector>
#include <glm/glm.hpp>

class Track {
    bool graphicsEnabled;
    std::vector<glm::vec2> points;
    int numSegments;

    int numIndices;
    unsigned int vao, vbo, ebo;

    bool loadPointsFromFile(const char* path);
    void generateGeometry();

public:
    Track(const char* path, bool enableGraphics);
    ~Track();

    void draw(int locModel, int locColor);
    glm::vec2 getPosition(float t);
    glm::vec2 getTangent(float t);
    glm::vec2 getNormal(float t);
    float getClosestT(const glm::vec2& position);
    std::vector<glm::vec3> getWaypoints(float currentT, int numWaypoints, float waypointSpacing);
    int getNumSegments() const { return numSegments; }
};

#endif // TRACK_H