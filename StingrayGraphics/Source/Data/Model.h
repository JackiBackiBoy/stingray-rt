#pragma once

#include "Core/Types.h"
#include "Graphics/GraphicsDevice.h"

#include <vector>
#include <glm/glm.hpp>

// TODO: Materials
struct SRMeshPrimitive {
	u32 baseVertex;
	u32 baseIndex;
	u32 numVertices;
	u32 numIndices;
};

struct SRMesh {
	u32 basePrimitive;
	u32 numPrimitives;
};

struct SRVertex {
	glm::vec3 position;
	//f32 pad;
	//glm::vec3 normal;
	//glm::vec2 texCoord;
};

struct SRModel {
	std::vector<SRMesh> meshes;
	std::vector<SRMeshPrimitive> primitives;
	u32 numMeshlets;

	SRBuffer vertexBuffer;
	SRBuffer indexBuffer;
	SRBuffer meshletBuffer;
	SRBuffer meshletVerticesBuffer;
	SRBuffer meshletTrianglesBuffer;
};

// TODO: Move elsewhere
struct SRMeshlet {
	u32 vertexOffset;
	u32 triangleOffset;
	u32 vertexCount;
	u32 triangleCount;
};

// TODO: Move to unified resource manager perhaps?
namespace SRModelLoader {
	void load_gltf(const char* path, SRModel& model, SRGraphicsDevice& gfxDevice);
}
