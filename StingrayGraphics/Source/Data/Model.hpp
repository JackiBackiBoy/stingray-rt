#pragma once

#include "Graphics/GraphicsDevice.hpp"

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

// TODO: Materials
struct SRMeshPrimitive {
	uint32_t baseVertex;
	uint32_t baseIndex;
	uint32_t numVertices;
	uint32_t numIndices;
};

struct SRMesh {
	uint32_t basePrimitive;
	uint32_t numPrimitives;
};

struct SRVertex {
	glm::vec3 position;
	//glm::vec3 normal;
	glm::vec2 texCoord;
};

struct SRModel {
	std::vector<SRMesh> meshes;
	std::vector<SRMeshPrimitive> primitives;
	uint32_t numMeshlets;

	SRBuffer vertexBuffer;
	SRBuffer indexBuffer;
	SRBuffer meshletBuffer;
};

// TODO: Move elsewhere
struct SRMeshlet {
	uint32_t vertexOffset;
	uint32_t triangleOffset;
	uint32_t vertexCount;
	uint32_t triangleCount;
};

// TODO: Move to unified resource manager perhaps?
namespace SRModelLoader {
	void load_gltf(const char* path, SRModel& model, SRGraphicsDevice& gfxDevice);
}
