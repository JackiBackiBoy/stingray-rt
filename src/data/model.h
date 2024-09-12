#pragma once

#include "../graphics/gfx_types.h"
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

struct Mesh {
	uint32_t numVertices = 0;
	uint32_t numIndices = 0;
	uint32_t baseVertex = 0;
	uint32_t baseIndex = 0;
	uint32_t albedoMapIndex = ~0;
	uint32_t normalMapIndex = ~0;
};

struct ModelVertex {
	glm::vec3 position = {};
	glm::vec3 normal = {};
	glm::vec3 tangent = {};
	glm::vec2 texCoord = {};
};

struct Model {
	std::vector<Mesh> meshes = {};
	std::vector<ModelVertex> vertices = {};
	std::vector<uint32_t> indices = {};
	//std::vector<Texture> materialTextures = {};

	Buffer vertexBuffer = {};
	Buffer indexBuffer = {};
};