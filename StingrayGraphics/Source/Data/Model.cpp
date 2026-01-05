#include "Model.h"
#include "Core/Logger.h"

// TODO: Get rid of stupid fastgltf library
#undef internal
#undef global
#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"
#include <meshoptimizer.h>

namespace {
	constexpr size_t MAX_VERTICES = 64;
	constexpr size_t MAX_TRIANGLES = 64;

	void load_gltf_mesh(
		SRModel& model,
		SRVertex* vertices,
		u32* indices,
		fastgltf::Asset& gltfAsset,
		fastgltf::Mesh& gltfMesh
	) {
		SRMesh mesh = {};
		mesh.basePrimitive = static_cast<u32>(model.primitives.size());
		mesh.numPrimitives = static_cast<u32>(gltfMesh.primitives.size());

		u32 baseVertex = 0;
		u32 baseIndex = 0;

		for (const auto& gltfPrimitive : gltfMesh.primitives) {
			SRVertex* verticesPtr = vertices + baseVertex;
			u32* indicesPtr = indices + baseIndex;

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
			//if (texCoordIt != gltfPrimitive.attributes.cend()) {
			//	fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
			//		gltfAsset,
			//		texCoordAcccesor,
			//		[&](fastgltf::math::fvec2 texCoord, size_t i) {
			//			verticesPtr[i].texCoord = glm::vec2(texCoord.x(), texCoord.y());
			//		}
			//	);
			//}

			if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedShort) {
				fastgltf::iterateAccessorWithIndex<u16>(
					gltfAsset,
					indexAccessor,
					[&](u16 index, size_t i) {
						indicesPtr[i] = static_cast<u32>(index);
					}
				);
			}
			else if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedInt) {
				fastgltf::iterateAccessorWithIndex<u32>(
					gltfAsset,
					indexAccessor,
					[&](u32 index, size_t i) {
						indicesPtr[i] = index;
					}
				);
			}

			const SRMeshPrimitive meshPrimitive = {
				.baseVertex = baseVertex,
				.baseIndex = baseIndex,
				.numVertices = static_cast<u32>(positionAccessor.count),
				.numIndices = static_cast<u32>(indexAccessor.count)
			};
			model.primitives.push_back(meshPrimitive);

			baseVertex += meshPrimitive.numVertices;
			baseIndex += meshPrimitive.numIndices;
		}

		model.meshes.push_back(mesh);
	} 
}

namespace SRModelLoader {
	void load_gltf(const char* path, SRModel& model, SRGFXDevice& gfxDevice) {
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
		u32* indices = new u32[numIndices];

		for (auto& gltfMesh : gltfAsset->meshes) {
			load_gltf_mesh(model, vertices, indices, gltfAsset.get(), gltfMesh);
		}

		// Generate meshlets
		// TODO: Might be better to just have ONE global meshlet buffer, look into this
		std::vector<SRMeshlet> meshlets;
		std::vector<u32> meshletVertices;
		std::vector<u8> meshletTriangles;
		
		size_t maxMeshlets = meshopt_buildMeshletsBound(numIndices, MAX_VERTICES, MAX_TRIANGLES);
		meshlets.resize(maxMeshlets);
		meshletVertices.resize(maxMeshlets * MAX_VERTICES);
		meshletTriangles.resize(maxMeshlets * MAX_TRIANGLES * 3);

		size_t meshletCount = meshopt_buildMeshlets(
			reinterpret_cast<meshopt_Meshlet*>(meshlets.data()),
			meshletVertices.data(),
			meshletTriangles.data(),
			indices,
			numIndices,
			reinterpret_cast<const f32*>(vertices),
			numVertices,
			sizeof(SRVertex),
			MAX_VERTICES,
			MAX_TRIANGLES,
			0.0f // TODO: Cone-weight, look into
		);

		SRMeshlet& lastMeshlet = meshlets[meshletCount - 1];
		meshletVertices.resize(lastMeshlet.vertexOffset + lastMeshlet.vertexCount);
		meshletTriangles.resize(lastMeshlet.triangleOffset + ((lastMeshlet.triangleCount * 3U + 3U) & ~3U));
		meshlets.resize(meshletCount);

		for (SRMeshlet& meshlet : meshlets) {
			meshopt_optimizeMeshlet(
				&meshletVertices[meshlet.vertexOffset],
				&meshletTriangles[meshlet.triangleOffset],
				meshlet.triangleCount,
				meshlet.vertexCount
			);
		}

		// TODO: AABS

		// Create buffers
		SRBufferInfo vertexBufferInfo = {
			.size = numVertices * sizeof(SRVertex),
			.stride = sizeof(SRVertex),
			.usage = SRUsage::Default,
			.bindFlags = SRBindFlag::VertexBuffer | SRBindFlag::ShaderResource,
			.miscFlags = SRMiscFlag::StructuredBuffer
		};
		SRBufferInfo indexBufferInfo = {
			.size = numIndices * sizeof(u32),
			.stride = sizeof(u32),
			.usage = SRUsage::Default,
			.bindFlags = SRBindFlag::IndexBuffer
		};
		SRBufferInfo meshletBufferInfo = {
			.size = meshlets.size() * sizeof(SRMeshlet),
			.stride = sizeof(SRMeshlet),
			.usage = SRUsage::Default,
			.bindFlags = SRBindFlag::ShaderResource,
			.miscFlags = SRMiscFlag::StructuredBuffer
		};
		SRBufferInfo meshletVerticesBufferInfo = {
			.size = meshletVertices.size() * sizeof(u32),
			.stride = sizeof(u32),
			.usage = SRUsage::Default,
			.bindFlags = SRBindFlag::ShaderResource,
			.miscFlags = SRMiscFlag::StructuredBuffer
		};
		SRBufferInfo meshletTrianglesBufferInfo = {
			.size = (meshletTriangles.size() / 3) * sizeof(u32),
			.stride = sizeof(u32),
			.usage = SRUsage::Default,
			.bindFlags = SRBindFlag::ShaderResource,
			.miscFlags = SRMiscFlag::StructuredBuffer
		};

		// Repack meshlet triangles
		std::vector<u32> meshletTrianglesU32;
		meshletTrianglesU32.reserve(meshletTriangles.size() / 3);

		for (SRMeshlet& meshlet : meshlets) {
			u32 triangleOffset = (u32)meshletTrianglesU32.size();

			for (u32 i = 0; i < meshlet.triangleCount; ++i) {
				u32 i0 = 3 * i + 0 + meshlet.triangleOffset;
				u32 i1 = 3 * i + 1 + meshlet.triangleOffset;
				u32 i2 = 3 * i + 2 + meshlet.triangleOffset;

				u8 vIdx0 = meshletTriangles[i0];
				u8 vIdx1 = meshletTriangles[i1];
				u8 vIdx2 = meshletTriangles[i2];

				u32 packed = ((u32)vIdx0 << 0) | ((u32)vIdx1 << 8) | ((u32)vIdx2 << 16);
				meshletTrianglesU32.push_back(packed);
			}

			meshlet.triangleOffset = triangleOffset;
		}

		SRGFX_CreateBuffer(&gfxDevice, &vertexBufferInfo, &model.vertexBuffer, vertices);
		SRGFX_CreateBuffer(&gfxDevice, &indexBufferInfo, &model.indexBuffer, indices);
		SRGFX_CreateBuffer(&gfxDevice, &meshletBufferInfo, &model.meshletBuffer, meshlets.data());
		SRGFX_CreateBuffer(&gfxDevice, &meshletVerticesBufferInfo, &model.meshletVerticesBuffer, meshletVertices.data());
		SRGFX_CreateBuffer(&gfxDevice, &meshletTrianglesBufferInfo, &model.meshletTrianglesBuffer, meshletTrianglesU32.data());
		model.numMeshlets = (u32)meshlets.size();

		delete[] vertices;
		delete[] indices;
		//delete[] indicesCopy;
	}
}
