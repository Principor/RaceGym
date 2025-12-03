#include "track.h"
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>
#include <iostream>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#define TRACK_WIDTH 12.0f

Track::Track(const char *path)
	: vao(0), vbo(0), ebo(0)
{
	if (loadPointsFromFile(path))
	{
		generateGeometry();
	}
}

Track::~Track()
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

void Track::draw(int locModel, int locColor)
{
	if (vao == 0)
		return;

	glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
	glUniform3f(locColor, 0.2f, 0.2f, 0.2f); // Dark gray/black for track

	glBindVertexArray(vao);
	glDrawElements(GL_TRIANGLE_STRIP, static_cast<GLsizei>(numIndices), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

glm::vec2 Track::getPosition(float t)
{
	int segment = static_cast<int>(t) % numSegments;
	glm::vec2 p0 = points[segment * 2];
	glm::vec2 p1 = points[segment * 2 + 1];
	glm::vec2 p2 = points[(segment * 2 + 2) % points.size()];
	float localT = t - std::floor(t);
	float invT = 1.0f - localT;
	return invT * invT * p0 + 2.0f * invT * localT * p1 + localT * localT * p2;
}

glm::vec2 Track::getTangent(float t)
{
	const float delta = 0.001f;
	glm::vec2 p1 = getPosition(t);
	glm::vec2 p2 = getPosition(t + delta);
	return glm::normalize(p2 - p1);
}

glm::vec2 Track::getNormal(float t)
{
	glm::vec2 tangent = getTangent(t);
	return glm::vec2(-tangent.y, tangent.x); // 90 degree rotation
}

static void skipWhitespace(const std::string &s, size_t &i)
{
	while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
		++i;
}

// Very minimal JSON loader expecting: { "points": [[x,y], [x,y], ...] }
bool Track::loadPointsFromFile(const char *path)
{
	if (!path)
		return false;
	std::ifstream f(path);
	if (!f.is_open())
		return false;
	std::stringstream buffer;
	buffer << f.rdbuf();
	std::string json = buffer.str();

	points.clear();

	size_t i = 0;
	skipWhitespace(json, i);
	if (i >= json.size() || json[i] != '{')
		return false;
	++i;
	// find "points"
	while (i < json.size())
	{
		skipWhitespace(json, i);
		if (json[i] == '}')
			break;
		if (json[i] == '"')
		{
			++i;
			size_t start = i;
			while (i < json.size() && json[i] != '"')
				++i;
			std::string key = json.substr(start, i - start);
			if (i < json.size())
				++i; // skip closing quote
			skipWhitespace(json, i);
			if (i >= json.size() || json[i] != ':')
				return false;
			++i;
			skipWhitespace(json, i);
			if (key == "points")
			{
				if (i >= json.size() || json[i] != '[')
					return false;
				++i; // start array
				// parse array of [x,y]
				while (i < json.size())
				{
					skipWhitespace(json, i);
					if (json[i] == ']')
					{
						++i;
						break;
					}
					if (json[i] != '[')
						return false;
					++i; // start point
					skipWhitespace(json, i);
					// parse x
					size_t numStart = i;
					while (i < json.size() && (std::isdigit(static_cast<unsigned char>(json[i])) || json[i] == '-' || json[i] == '+' || json[i] == '.' || json[i] == 'e' || json[i] == 'E'))
						++i;
					float x = std::stof(json.substr(numStart, i - numStart));
					skipWhitespace(json, i);
					if (i >= json.size() || json[i] != ',')
						return false;
					++i;
					skipWhitespace(json, i);
					// parse y
					numStart = i;
					while (i < json.size() && (std::isdigit(static_cast<unsigned char>(json[i])) || json[i] == '-' || json[i] == '+' || json[i] == '.' || json[i] == 'e' || json[i] == 'E'))
						++i;
					float y = std::stof(json.substr(numStart, i - numStart));
					skipWhitespace(json, i);
					if (i >= json.size() || json[i] != ']')
						return false;
					++i; // end point
					points.emplace_back(x, y);
					skipWhitespace(json, i);
					if (json[i] == ',')
					{
						++i;
						continue;
					}
				}
			}
			else
			{
				// skip value for other keys
				// naive skip: if starts with '"', read string; '[' or '{' skip balanced; else read token
				if (i < json.size() && json[i] == '"')
				{
					++i;
					while (i < json.size() && json[i] != '"')
						++i;
					if (i < json.size())
						++i;
				}
				else if (i < json.size() && (json[i] == '[' || json[i] == '{'))
				{
					char open = json[i];
					char close = (open == '[') ? ']' : '}';
					++i;
					int depth = 1;
					while (i < json.size() && depth > 0)
					{
						if (json[i] == open)
							++depth;
						else if (json[i] == close)
							--depth;
						++i;
					}
				}
				else
				{
					while (i < json.size() && json[i] != ',' && json[i] != '}')
						++i;
				}
			}
			skipWhitespace(json, i);
			if (i < json.size() && json[i] == ',')
			{
				++i;
			}
			continue;
		}
		++i;
	}

	numSegments = points.size() / 2;

	return !points.empty();
}

void Track::generateGeometry()
{
	if (points.empty())
		return;

	int resolution = numSegments * 20; // 20 samples per segment

	// Generate VBO
	std::vector<float> vertexData;
	for (int i = 0; i < resolution; ++i)
	{
		glm::vec2 p;

		// LHS
		float t0 = static_cast<float>(i) / static_cast<float>(resolution - 1) * static_cast<float>(numSegments);
		p = getPosition(t0) + getNormal(t0) * TRACK_WIDTH / 2.0f; // Offset by 2 units to the side
		vertexData.push_back(p.x);
		vertexData.push_back(0.0f); // Same height as ground plane
		vertexData.push_back(p.y);

		// RHS
		float t1 = (static_cast<float>(i) + 0.5f) / static_cast<float>(resolution - 1) * static_cast<float>(numSegments);
		p = getPosition(t1) - getNormal(t1) * TRACK_WIDTH / 2.0f; // Offset by 2 units to the side
		vertexData.push_back(p.x);
		vertexData.push_back(0.0f); // Same height as ground plane
		vertexData.push_back(p.y);
	}

	// Generate EBO
	numIndices = resolution * 2;
	std::vector<unsigned int> indices(numIndices);
	for (unsigned int i = 0; i < numIndices; ++i)
	{
		indices[i] = i;
	}

	// Create VAO and bind buffers within it
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);

	glBindVertexArray(vao);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	glBindVertexArray(0);
}
