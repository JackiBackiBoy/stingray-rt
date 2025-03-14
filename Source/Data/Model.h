#pragma once

#include "Graphics/GraphicsTypes.h"
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace SR {
	struct MeshPrimitive {
		uint32_t numVertices = 0;
		uint32_t numIndices = 0;
		uint32_t baseVertex = 0;
		uint32_t baseIndex = 0;
	};

	struct Mesh {
		std::vector<MeshPrimitive> primitives = {};
	};

	struct ModelVertex {
		glm::vec3 position = {};
		glm::vec3 normal = {};
		glm::vec3 tangent = {};
		glm::vec2 texCoord = {};
		uint32_t matIndex = 0;
		float pad2;
	};

	struct Model {
		std::vector<Mesh> meshes = {};
		std::vector<ModelVertex> vertices = {};
		std::vector<uint32_t> indices = {};
		std::vector<Texture> materialTextures = {};

		Buffer vertexBuffer = {};
		Buffer indexBuffer = {};
	};
}
