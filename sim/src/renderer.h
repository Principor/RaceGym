#ifndef RACEGYM_RENDERER_H
#define RACEGYM_RENDERER_H

#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

class Track;
class Vehicle;

struct Mesh {
    unsigned int vao;
    unsigned int vbo;
    unsigned int ebo;
    int numIndices;
};

class Renderer {
public:
	// Renderer uses an internal singleton; API is static and stateful.
	static bool init();
	static bool is_initialized();
	static void shutdown();
	static void render_step(Track* track, const std::vector<Vehicle*>& vehicles, bool& running);

    static Mesh createMesh(const float* vertices, int numVertices, const unsigned int* indices, int numIndices);
    static void drawMesh(const Mesh& mesh, glm::mat4 modelMatrix, glm::vec3 colour, int drawMode = GL_TRIANGLES);
    static void destroyMesh(Mesh& mesh);
};

#endif // RACEGYM_RENDERER_H
