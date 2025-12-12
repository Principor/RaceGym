#include "track.h"
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>
#include <iostream>
#include <limits>
#include <vector>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#define TRACK_WIDTH 12.0f

Track::Track(const char *path)
{
	if (loadPointsFromFile(path))
	{
		generateGeometry();
	}
}

Track::~Track()
{
	if(Renderer::is_initialized())
		Renderer::destroyMesh(trackMesh);
}

void Track::draw(int locModel, int locColor)
{
	if(!Renderer::is_initialized())
		return;

	Renderer::drawMesh(trackMesh, glm::mat4(1.0f), glm::vec3(0.2f, 0.2f, 0.2f), GL_TRIANGLE_STRIP);
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
	if (!Renderer::is_initialized())
		return;

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
	int numIndices = resolution * 2;
	std::vector<unsigned int> indices(numIndices);
	for (unsigned int i = 0; i < numIndices; ++i)
	{
		indices[i] = i;
	}

	trackMesh = Renderer::createMesh(vertexData.data(), static_cast<int>(vertexData.size() / 3), indices.data(), static_cast<int>(indices.size()));
}

float Track::getClosestT(const glm::vec2 &position)
{
	float globalClosestT = 0.0f;
	float globalMinDistSq = std::numeric_limits<float>::max();

	// Check each segment independently
	for (int seg = 0; seg < numSegments; ++seg)
	{
		// Get the three control points for this quadratic Bezier segment
		glm::vec2 p0 = points[seg * 2];
		glm::vec2 p1 = points[seg * 2 + 1];
		glm::vec2 p2 = points[(seg * 2 + 2) % points.size()];

		// Bezier curve: B(t) = (1-t)^2 * p0 + 2*(1-t)*t * p1 + t^2 * p2
		// We want to minimize |B(t) - position|^2
		// This is equivalent to finding where d/dt |B(t) - position|^2 = 0
		
		// Let Q = position
		// B(t) = p0 + 2t(p1 - p0) + t^2(p0 - 2p1 + p2)
		// B(t) - Q = (p0 - Q) + 2t(p1 - p0) + t^2(p0 - 2p1 + p2)
		
		// |B(t) - Q|^2 = dot(B(t) - Q, B(t) - Q)
		// d/dt |B(t) - Q|^2 = 2 * dot(B(t) - Q, B'(t))
		
		// B'(t) = 2(p1 - p0) + 2t(p0 - 2p1 + p2)
		
		// Setting derivative to zero:
		// dot(B(t) - Q, B'(t)) = 0
		
		// This expands to a cubic equation in t
		// For simplicity and robustness, we'll use iterative refinement with Newton's method
		// starting from multiple sample points
		
		glm::vec2 a = p0 - 2.0f * p1 + p2;  // coefficient of t^2
		glm::vec2 b = 2.0f * (p1 - p0);      // coefficient of t
		glm::vec2 c = p0 - position;         // constant term (offset from position)
		
		// Sample initial candidates and refine with Newton's method
		float candidates[11]; // Start, end, and 9 intermediate points
		int numCandidates = 11;
		for (int i = 0; i < numCandidates; ++i)
		{
			candidates[i] = static_cast<float>(i) / static_cast<float>(numCandidates - 1);
		}
		
		float segmentClosestT = 0.0f;
		float segmentMinDistSq = std::numeric_limits<float>::max();
		
		for (int i = 0; i < numCandidates; ++i)
		{
			float t = candidates[i];
			
			// Newton's method iterations to find local minimum
			for (int iter = 0; iter < 5; ++iter)
			{
				// B(t) - Q = c + t*b + t^2*a
				glm::vec2 Bt = c + t * b + t * t * a;
				
				// B'(t) = b + 2*t*a
				glm::vec2 dBt = b + 2.0f * t * a;
				
				// f(t) = dot(B(t) - Q, B'(t))
				float f = glm::dot(Bt, dBt);
				
				// f'(t) = dot(B'(t), B'(t)) + dot(B(t) - Q, B''(t))
				// B''(t) = 2*a
				float df = glm::dot(dBt, dBt) + glm::dot(Bt, 2.0f * a);
				
				// Avoid division by zero
				if (std::abs(df) < 1e-6f)
					break;
				
				// Newton's step
				float newT = t - f / df;
				
				// Clamp to [0, 1]
				newT = std::max(0.0f, std::min(1.0f, newT));
				
				// Check convergence
				if (std::abs(newT - t) < 1e-6f)
					break;
				
				t = newT;
			}
			
			// Evaluate distance at this t
			glm::vec2 Bt = c + t * b + t * t * a;
			float distSq = glm::dot(Bt, Bt);
			
			if (distSq < segmentMinDistSq)
			{
				segmentMinDistSq = distSq;
				segmentClosestT = t;
			}
		}
		
		// Convert local t to global t
		float globalT = static_cast<float>(seg) + segmentClosestT;
		
		if (segmentMinDistSq < globalMinDistSq)
		{
			globalMinDistSq = segmentMinDistSq;
			globalClosestT = globalT;
		}
	}

	return globalClosestT;
}

std::vector<glm::vec3> Track::getWaypoints(float currentT, int numWaypoints, float waypointSpacing)
{
	std::vector<glm::vec3> waypoints;
	waypoints.reserve(numWaypoints * 2);

	const float halfWidth = TRACK_WIDTH / 2.0f;

	for (int i = 0; i < numWaypoints; ++i)
	{
		float t = currentT + static_cast<float>(i) * waypointSpacing;
		
		// Wrap around track
		while (t >= static_cast<float>(numSegments))
			t -= static_cast<float>(numSegments);
		while (t < 0.0f)
			t += static_cast<float>(numSegments);

		glm::vec2 centerPos = getPosition(t);
		glm::vec2 normal = getNormal(t);

		// Left side of track
		glm::vec2 leftPos = centerPos + normal * halfWidth;
		waypoints.emplace_back(leftPos.x, 0.0f, leftPos.y);

		// Right side of track
		glm::vec2 rightPos = centerPos - normal * halfWidth;
		waypoints.emplace_back(rightPos.x, 0.0f, rightPos.y);
	}

	return waypoints;
}