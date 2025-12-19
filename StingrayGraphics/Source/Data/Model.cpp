#include "Model.h"
#include "Core/Logger.h"

#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"
#include <meshoptimizer.h>

namespace {
	constexpr size_t MAX_VERTICES = 64;
	constexpr size_t MAX_TRIANGLES = 64;

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
		// TODO: Might be better to just have ONE global meshlet buffer, look into this
		std::vector<SRMeshlet> meshlets;
		std::vector<uint32_t> meshletVertices;
		std::vector<uint8_t> meshletTriangles;
		
		const size_t maxMeshlets = meshopt_buildMeshletsBound(numIndices, MAX_VERTICES, MAX_TRIANGLES);
		meshlets.resize(maxMeshlets);
		meshletVertices.resize(maxMeshlets * MAX_VERTICES);
		meshletTriangles.resize(maxMeshlets * MAX_TRIANGLES * 3);

		const size_t meshletCount = meshopt_buildMeshlets(
			reinterpret_cast<meshopt_Meshlet*>(meshlets.data()),
			meshletVertices.data(),
			meshletTriangles.data(),
			indices,
			numIndices,
			reinterpret_cast<const float*>(vertices),
			numVertices,
			sizeof(SRVertex),
			MAX_VERTICES,
			MAX_TRIANGLES,
			0.0f // TODO: Cone-weight, look into
		);

		const SRMeshlet& lastMeshlet = meshlets[meshletCount - 1];
		meshletVertices.resize(lastMeshlet.vertexOffset + lastMeshlet.vertexCount);
		meshletTriangles.resize(lastMeshlet.triangleOffset + ((lastMeshlet.triangleCount * 3U + 3U) & ~3U));
		meshlets.resize(meshletCount);

		for (const SRMeshlet& meshlet : meshlets) {
			meshopt_optimizeMeshlet(
				&meshletVertices[meshlet.vertexOffset],
				&meshletTriangles[meshlet.triangleOffset],
				meshlet.triangleCount,
				meshlet.vertexCount
			);
		}

		// TODO: AABS

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
		const SRBufferInfo meshletBufferInfo = {
			.size = meshlets.size() * sizeof(SRMeshlet),
			.stride = sizeof(SRMeshlet),
			.usage = SRUsage::Default,
			.bindFlags = SRBindFlag::ShaderResource,
			.miscFlags = SRMiscFlag::StructuredBuffer
		};

		gfxDevice.create_buffer(vertexBufferInfo, model.vertexBuffer, vertices);
		gfxDevice.create_buffer(indexBufferInfo, model.indexBuffer, indices);
		gfxDevice.create_buffer(meshletBufferInfo, model.meshletBuffer, meshlets.data());
		model.numMeshlets = (uint32_t)meshlets.size();

		delete[] vertices;
		delete[] indices;
		//delete[] indicesCopy;
	}
}
