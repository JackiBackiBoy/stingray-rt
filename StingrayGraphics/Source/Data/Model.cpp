#include "Model.hpp"
#include "Core/Logger.hpp"

#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"
#include <meshoptimizer.h>

namespace {
	void load_gltf_mesh(
		SRModel& model,
		SRVertex* vertices,
		uint32_t* indices,
		fastgltf::Asset& gltfAsset,
		fastgltf::Mesh& gltfMesh
	) {
		SRMesh mesh = {};
		mesh.basePrimitive = static_cast<uint32_t>(model.primitives.size());
		mesh.numPrimitives = static_cast<uint32_t>(gltfMesh.primitives.size());

		uint32_t baseVertex = 0;
		uint32_t baseIndex = 0;

		for (const auto& gltfPrimitive : gltfMesh.primitives) {
			SRVertex* verticesPtr = vertices + baseVertex;
			uint32_t* indicesPtr = indices + baseIndex;

			auto* positionIt = gltfPrimitive.findAttribute("POSITION");
			// TODO: Handle TEXCOORD indices better, i.e. TEXCOORD_0, TEXCOORD_1 and so on
			auto* texCoordIt = gltfPrimitive.findAttribute("TEXCOORD_0");

			assert(gltfPrimitive.indicesAccessor.has_value());

			// TODO: Handle materials

			auto& positionAccessor = gltfAsset.accessors[positionIt->accessorIndex];
			auto& texCoordAcccesor = gltfAsset.accessors[texCoordIt->accessorIndex];
			auto& indexAccessor = gltfAsset.accessors[gltfPrimitive.indicesAccessor.value()];
			assert(positionAccessor.bufferViewIndex.has_value());
			assert(texCoordAcccesor.bufferViewIndex.has_value());
			assert(indexAccessor.bufferViewIndex.has_value());

			// TODO: Interpret data with LH coordinate system in mind
			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
				gltfAsset,
				positionAccessor,
				[&](fastgltf::math::fvec3 pos, size_t i) {
					verticesPtr[i].position = glm::vec3(pos.x(), pos.z(), pos.y());
				}
			);
			if (texCoordIt != gltfPrimitive.attributes.cend()) {
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
					gltfAsset,
					texCoordAcccesor,
					[&](fastgltf::math::fvec2 texCoord, size_t i) {
						verticesPtr[i].texCoord = glm::vec2(texCoord.x(), texCoord.y());
					}
				);
			}

			if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedShort) {
				fastgltf::iterateAccessorWithIndex<uint16_t>(
					gltfAsset,
					indexAccessor,
					[&](uint16_t index, size_t i) {
						indicesPtr[i] = static_cast<uint32_t>(index);
					}
				);
			}
			else if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedInt) {
				fastgltf::iterateAccessorWithIndex<uint32_t>(
					gltfAsset,
					indexAccessor,
					[&](uint32_t index, size_t i) {
						indicesPtr[i] = index;
					}
				);
			}

			const SRMeshPrimitive meshPrimitive = {
				.baseVertex = baseVertex,
				.baseIndex = baseIndex,
				.numVertices = static_cast<uint32_t>(positionAccessor.count),
				.numIndices = static_cast<uint32_t>(indexAccessor.count)
			};
			model.primitives.push_back(meshPrimitive);

			baseVertex += meshPrimitive.numVertices;
			baseIndex += meshPrimitive.numIndices;
		}

		model.meshes.push_back(mesh);
	} 
}

namespace SRModelLoader {
	void load_gltf(const char* path, SRModel& model, SRGraphicsDevice& gfxDevice) {
		fastgltf::Parser gltfParser = fastgltf::Parser();

		const fastgltf::Options gltfOptions = (
			fastgltf::Options::DontRequireValidAssetMember |
			fastgltf::Options::LoadExternalBuffers |
			fastgltf::Options::LoadExternalImages
		);

		auto fileSystemPath = std::filesystem::path(path);
		auto gltfFile = fastgltf::MappedGltfFile::FromPath(fileSystemPath);
		if (!bool(gltfFile)) {
			//SRLOG_ERROR("Failed to load GLTF file: %s", fastgltf::getErrorMessage(gltfFile.error()));
			return;
		}

		auto gltfAsset = gltfParser.loadGltf(gltfFile.get(), fileSystemPath.parent_path(), gltfOptions);
		if (gltfAsset.error() != fastgltf::Error::None) {
			//SRLOG_ERROR("Failed to load GLTF file: %s", fastgltf::getErrorMessage(gltfAsset.error()));
			return;
		}

		model.meshes.reserve(gltfAsset->meshes.size());

		// Pre-allocate index and vertex buffer data
		size_t numVertices = 0;
		size_t numIndices = 0;
		size_t numPrimitives = 0;
		for (const auto& gltfMesh : gltfAsset->meshes) {
			for (const auto& gltfPrimitive : gltfMesh.primitives) {
				auto* positionIt = gltfPrimitive.findAttribute("POSITION");
				auto& positionAccessor = gltfAsset->accessors[positionIt->accessorIndex];
				auto& indexAccessor = gltfAsset->accessors[gltfPrimitive.indicesAccessor.value()];

				++numPrimitives;
				numVertices += positionAccessor.count;
				numIndices += indexAccessor.count;
			}
		}

		model.primitives.reserve(numPrimitives);
		SRVertex* vertices = new SRVertex[numVertices];
		uint32_t* indices = new uint32_t[numIndices];

		for (auto& gltfMesh : gltfAsset->meshes) {
			load_gltf_mesh(model, vertices, indices, gltfAsset.get(), gltfMesh);
		}

		// Generate meshlets
		// TODO: We will have to look into if this should be done per mesh, or per model
		// For now we assume ONE mesh per model
		//std::vector<uint32_t> remap(numIndices);
		//const size_t vertexCount = meshopt_generateVertexRemap(
		//	remap.data(),
		//	indices,
		//	numIndices,
		//	vertices,
		//	numVertices,
		//	sizeof(SRVertex)
		//);

		//meshopt_remapIndexBuffer(indices, indices, numIndices, remap.data());
		//meshopt_remapVertexBuffer(vertices, vertices, numVertices, sizeof(SRVertex), remap.data());

		//meshopt_optimizeVertexCache(indices, indices, numIndices, numVertices);
		//meshopt_optimizeOverdraw(indices, indices, numIndices, (const float*)vertices, numVertices, sizeof(SRVertex), 1.05f);

		//uint32_t* indicesCopy = new uint32_t[numIndices];
		//std::memcpy(indicesCopy, indices, numIndices * sizeof(uint32_t));
		//meshopt_optimizeVertexFetchRemap(remap.data(), indices, numIndices, numVertices);

		//meshopt_remapIndexBuffer(indices, )

		// Create buffers
		const SRBufferInfo vertexBufferInfo = {
			.size = numVertices * sizeof(SRVertex),
			.stride = sizeof(SRVertex),
			.usage = SRUsage::Default,
			.bindFlags = SRBindFlag::VertexBuffer
		};
		const SRBufferInfo indexBufferInfo = {
			.size = numIndices * sizeof(uint32_t),
			.stride = sizeof(uint32_t),
			.usage = SRUsage::Default,
			.bindFlags = SRBindFlag::IndexBuffer
		};

		gfxDevice.create_buffer(vertexBufferInfo, model.vertexBuffer, vertices);
		gfxDevice.create_buffer(indexBufferInfo, model.indexBuffer, indices);

		delete[] vertices;
		delete[] indices;
		//delete[] indicesCopy;
	}
}
