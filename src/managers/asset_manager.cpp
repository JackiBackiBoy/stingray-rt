#include "asset_manager.h"

#include "../platform.h"
#include <tiny_gltf.h>
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <stdexcept>

GFXDevice* g_GfxDevice = nullptr;

struct AssetInternal {
	Model model = {};
	Texture texture = {};
};

namespace assetmanager {
	enum class DataType : uint8_t {
		UNKNOWN,
		IMAGE,
		MODEL,
		SOUND,
	};

	GLOBAL_VARIABLE std::unordered_map<std::string, std::weak_ptr<AssetInternal>> g_Assets = {};
	GLOBAL_VARIABLE tinygltf::TinyGLTF g_GltfLoader = {};

	static const std::unordered_map<std::string, DataType> g_Types = {
		{ "jpg", DataType::IMAGE },
		{ "jpeg", DataType::IMAGE },
		{ "png", DataType::IMAGE },
		{ "hdr", DataType::IMAGE },
		{ "gltf", DataType::MODEL },
		{ "wav", DataType::SOUND },
		{ "ogg", DataType::SOUND },
	};

	void initialize(GFXDevice& gfxDevice) {
		g_GfxDevice = &gfxDevice;
	}

	void destroy() {

	}

	std::unique_ptr<Model> create_plane(float width, float depth) {
		assert(width != 0 && depth != 0);

		const float halfWidth = width * 0.5f;
		const float halfDepth = depth * 0.5f;

		std::unique_ptr<Model> model = std::make_unique<Model>();
		model->vertices = {
			ModelVertex{ // top left
				.position = { -halfWidth, 0.0f, halfDepth },
				.normal = { 0.0f, 1.0f, 0.0f },
				.tangent = { 1.0f, 0.0f, 0.0f },
				.texCoord = { 0.0f, 1.0f }
			},
			ModelVertex{ // bottom left
				.position = { -halfWidth, 0.0f, -halfDepth },
				.normal = { 0.0f, 1.0f, 0.0f },
				.tangent = { 1.0f, 0.0f, 0.0f },
				.texCoord = { 0.0f, 0.0f }
			},
			ModelVertex{ // bottom right
				.position = { halfWidth, 0.0f, -halfDepth },
				.normal = { 0.0f, 1.0f, 0.0f },
				.tangent = { 1.0f, 0.0f, 0.0f },
				.texCoord = { 1.0f, 0.0f }
			},
			ModelVertex{ // top right
				.position = { halfWidth, 0.0f, halfDepth },
				.normal = { 0.0f, 1.0f, 0.0f },
				.tangent = { 1.0f, 0.0f, 0.0f },
				.texCoord = { 1.0f, 1.0f }
			}
		};
		model->indices = { 0, 1, 2, 2, 3, 0 };

		Mesh mesh = {};
		mesh.numVertices = static_cast<uint32_t>(model->vertices.size());
		mesh.numIndices = static_cast<uint32_t>(model->indices.size());
		mesh.baseVertex = 0;
		mesh.baseIndex = 0;
		model->meshes.push_back(mesh);

		const BufferInfo vertexBufferInfo = {
			.size = sizeof(ModelVertex) * model->vertices.size(),
			.stride = sizeof(ModelVertex),
			.usage = Usage::DEFAULT,
			.bindFlags = BindFlag::VERTEX_BUFFER
		};

		const BufferInfo indexBufferInfo = {
			.size = sizeof(uint32_t) * model->indices.size(),
			.stride = sizeof(uint32_t),
			.usage = Usage::DEFAULT,
			.bindFlags = BindFlag::INDEX_BUFFER
		};

		g_GfxDevice->create_buffer(vertexBufferInfo, model->vertexBuffer, model->vertices.data());
		g_GfxDevice->create_buffer(indexBufferInfo, model->indexBuffer, model->indices.data());

		return model;
	}

	std::unique_ptr<Model> create_sphere(float radius, int latitudes, int longitudes) {
		std::unique_ptr<Model> model = std::make_unique<Model>();
		model->vertices.reserve(static_cast<size_t>((latitudes + 1) * (longitudes + 1)));
		model->indices.reserve(static_cast<size_t>(6 * (longitudes) * (latitudes - 1)));

		const float latAngStep = glm::pi<float>() / latitudes;
		const float lonAngStep = glm::two_pi<float>() / longitudes;
		float latAng;
		float lonAng;
		float rCosLatAng;

		// Vertices
		for (int lon = 0; lon <= longitudes; ++lon) {
			lonAng = lon * lonAngStep;

			for (int lat = 0; lat <= latitudes; ++lat) {
				latAng = glm::half_pi<float>() - lat * latAngStep;
				rCosLatAng = radius * cosf(latAng);

				ModelVertex vertex = {};
				vertex.position.x = -rCosLatAng * sinf(lonAng);
				vertex.position.y = radius * sinf(latAng);
				vertex.position.z = rCosLatAng * cosf(lonAng);
				vertex.normal = glm::normalize(vertex.position);
				vertex.texCoord.x = (float)lon / longitudes;
				vertex.texCoord.y = (float)lat / latitudes;

				model->vertices.push_back(vertex);
			}
		}

		// Indices
		//   k1---k2
		//    |  / |
		//    | /  |
		//    |/   |
		// k1+1---k2+1
		uint32_t k1, k2;
		for (int lon = 0; lon < longitudes; ++lon) {
			k1 = lon * (latitudes + 1);
			k2 = k1 + latitudes + 1;

			for (int lat = 0; lat < latitudes; ++lat, ++k1, ++k2) {
				if (lat != 0) {
					model->indices.push_back(k1);
					model->indices.push_back(k1 + 1);
					model->indices.push_back(k2);
				}

				if (lat != (latitudes - 1)) {
					model->indices.push_back(k2);
					model->indices.push_back(k1 + 1);
					model->indices.push_back(k2 + 1);
				}
			}
		}

		Mesh mesh = {};
		mesh.numVertices = static_cast<uint32_t>(model->vertices.size());
		mesh.numIndices = static_cast<uint32_t>(model->indices.size());
		mesh.baseVertex = 0;
		mesh.baseIndex = 0;
		model->meshes.push_back(mesh);

		const BufferInfo vertexBufferInfo = {
			.size = sizeof(ModelVertex) * model->vertices.size(),
			.stride = sizeof(ModelVertex),
			.usage = Usage::DEFAULT,
			.bindFlags = BindFlag::VERTEX_BUFFER
		};

		const BufferInfo indexBufferInfo = {
			.size = sizeof(uint32_t) * model->indices.size(),
			.stride = sizeof(uint32_t),
			.usage = Usage::DEFAULT,
			.bindFlags = BindFlag::INDEX_BUFFER
		};

		g_GfxDevice->create_buffer(vertexBufferInfo, model->vertexBuffer, model->vertices.data());
		g_GfxDevice->create_buffer(indexBufferInfo, model->indexBuffer, model->indices.data());

		return model;
	}

	INTERNAL void load_model(Asset& outAsset, const std::string& path, std::shared_ptr<AssetInternal> asset) {
		// TODO: Loading models is a big question mark in this engine, because at some point
		// we will use our own model format. But that is at the time of writing not something
		// that is of high importance. Just try to keep in mind that this will very likely change.
		// For now however, we will only be using GLTF.

		tinygltf::Model gltfModel = {};
		std::string error = {};
		std::string warning = {};

		if (!g_GltfLoader.LoadASCIIFromFile(&gltfModel, &error, &warning, path)) {
			throw std::runtime_error("GLTF ERROR: Failed to load GLFT model!");
		}

		asset->model.meshes.resize(gltfModel.meshes.size());
		//asset->model.materialTextures.resize(gltfModel.images.size());

		// Load material textures
		for (size_t i = 0; i < gltfModel.images.size(); ++i) {
			const tinygltf::Image& gltfImage = gltfModel.images[i];

			//const TextureInfo textureInfo = {
			//	.width = static_cast<uint32_t>(gltfImage.width),
			//	.height = static_cast<uint32_t>(gltfImage.height),
			//	.format = Format::R8G8B8A8_UNORM,
			//	.usage = Usage::DEFAULT,
			//	.bindFlags = BindFlag::SHADER_RESOURCE
			//};

			//const SubresourceData textureSubresource = {
			//	.data = gltfImage.image.data(),
			//	.rowPitch = 4U * static_cast<uint32_t>(gltfImage.width),
			//};

			//device.createTexture(textureInfo, asset->model.materialTextures[i], &textureSubresource);
		}

		uint32_t baseVertex = 0;
		uint32_t baseIndex = 0;

		// Load model data
		for (size_t i = 0; i < gltfModel.meshes.size(); ++i) {
			tinygltf::Mesh gltfMesh = gltfModel.meshes[i];
			Mesh& mesh = asset->model.meshes[i];
			mesh.baseVertex = baseVertex;
			mesh.baseIndex = baseIndex;

			// Nodes (used for positioning multiple meshes)
			glm::mat4 translation = glm::mat4(1.0f); // default identity matrix
			const std::vector<double> translationData = gltfModel.nodes[i].translation;

			if (!translationData.empty()) {
				translation = glm::translate({ 1.0f }, glm::vec3(translationData[2], translationData[1], translationData[0]));
			}

			// TODO: We should probably support meshes having more than 1 primitive,
			// as of right now, we do not...
			for (size_t j = 0; j < gltfMesh.primitives.size(); ++j) {
				tinygltf::Primitive& gltfPrimitive = gltfMesh.primitives[j];

				// Position
				const tinygltf::Accessor& posAccessor = gltfModel.accessors[gltfPrimitive.attributes["POSITION"]];
				const tinygltf::BufferView& posBufferView = gltfModel.bufferViews[posAccessor.bufferView];
				const tinygltf::Buffer& posBuffer = gltfModel.buffers[posBufferView.buffer];

				const float* positions = reinterpret_cast<const float*>(
					&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset]
					);

				// Normals
				const tinygltf::Accessor& normalAccessor = gltfModel.accessors[gltfPrimitive.attributes["NORMAL"]];
				const tinygltf::BufferView& normalBufferView = gltfModel.bufferViews[normalAccessor.bufferView];
				const tinygltf::Buffer& normalBuffer = gltfModel.buffers[normalBufferView.buffer];

				const float* normals = reinterpret_cast<const float*>(
					&posBuffer.data[normalBufferView.byteOffset + normalAccessor.byteOffset]
					);

				// Tangents
				const tinygltf::Accessor& tangentAccessor = gltfModel.accessors[gltfPrimitive.attributes["TANGENT"]];
				const tinygltf::BufferView& tangentBufferView = gltfModel.bufferViews[tangentAccessor.bufferView];
				const tinygltf::Buffer& tangentBuffer = gltfModel.buffers[tangentBufferView.buffer];

				const float* tangents = reinterpret_cast<const float*>(
					&tangentBuffer.data[tangentBufferView.byteOffset + tangentAccessor.byteOffset]
					);

				// Texture coordinates
				const tinygltf::Accessor& texCoordAccessor = gltfModel.accessors[gltfPrimitive.attributes["TEXCOORD_0"]];
				const tinygltf::BufferView& texCoordBufferView = gltfModel.bufferViews[texCoordAccessor.bufferView];
				const tinygltf::Buffer& texCoordBuffer = gltfModel.buffers[texCoordBufferView.buffer];

				const float* texCoords = reinterpret_cast<const float*>(
					&texCoordBuffer.data[texCoordBufferView.byteOffset + texCoordAccessor.byteOffset]
					);

				// Indices
				const tinygltf::Accessor& indicesAccessor = gltfModel.accessors[gltfPrimitive.indices];
				const tinygltf::BufferView& indexBufferView = gltfModel.bufferViews[indicesAccessor.bufferView];
				const tinygltf::Buffer& indexBuffer = gltfModel.buffers[indexBufferView.buffer];

				const uint16_t* indices = reinterpret_cast<const uint16_t*>(
					&indexBuffer.data[indexBufferView.byteOffset + indicesAccessor.byteOffset]
					);

				for (size_t k = 0; k < posAccessor.count; ++k) {
					ModelVertex vertex{};

					vertex.position = {
						positions[k * 3 + 2],
						positions[k * 3 + 1],
						positions[k * 3 + 0]
					};

					vertex.position = glm::vec3(translation * glm::vec4(vertex.position, 1.0f));

					vertex.normal = {
						normals[k * 3 + 2],
						normals[k * 3 + 1],
						normals[k * 3 + 0]
					};

					vertex.tangent = {
						tangents[k * 4 + 0],
						tangents[k * 4 + 1],
						tangents[k * 4 + 2]
					};

					vertex.texCoord = {
						texCoords[k * 2],
						texCoords[k * 2 + 1]
					};

					asset->model.vertices.push_back(vertex);
				}

				for (size_t k = 0; k < indicesAccessor.count; ++k) {
					asset->model.indices.push_back(static_cast<uint32_t>(indices[k]));
				}

				mesh.numVertices = static_cast<uint32_t>(posAccessor.count);
				mesh.numIndices = static_cast<uint32_t>(indicesAccessor.count);

				// Materials
				if (gltfPrimitive.material == -1) {
					continue;
				}

				const tinygltf::Material& material = gltfModel.materials[gltfPrimitive.material];
				mesh.albedoMapIndex = (uint32_t)material.pbrMetallicRoughness.baseColorTexture.index;
				mesh.normalMapIndex = (uint32_t)material.normalTexture.index;
			}

			baseVertex = static_cast<uint32_t>(asset->model.vertices.size());
			baseIndex = static_cast<uint32_t>(asset->model.indices.size());

		}

		// Create related buffers for the GPU
		const BufferInfo vertexBufferInfo = {
			.size = asset->model.vertices.size() * sizeof(ModelVertex),
			.stride = sizeof(ModelVertex),
			.usage = Usage::DEFAULT,
			.bindFlags = BindFlag::VERTEX_BUFFER, // TODO: This is hardcoded here for Ray-Tracing shader, should probably be altered
			.miscFlags = MiscFlag::BUFFER_STRUCTURED
		};

		const BufferInfo indexBufferInfo = {
			.size = asset->model.indices.size() * sizeof(uint32_t),
			.stride = sizeof(uint32_t),
			.usage = Usage::DEFAULT,
			.bindFlags = BindFlag::INDEX_BUFFER, // TODO: This is hardcoded here for Ray-Tracing shader, should probably be altered
			.miscFlags = MiscFlag::BUFFER_STRUCTURED
		};

		g_GfxDevice->create_buffer(vertexBufferInfo, asset->model.vertexBuffer, asset->model.vertices.data());
		g_GfxDevice->create_buffer(indexBufferInfo, asset->model.indexBuffer, asset->model.indices.data());

		outAsset.internalState = asset;
	}

	INTERNAL void load_texture(Asset& outAsset, const std::string& path, std::shared_ptr<AssetInternal> asset) {
		int width = 0;
		int height = 0;
		int channels = 0;

		// TODO: For now, all images will be converted to RGBA format, which might not always be desired
		uint8_t* data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

		if (!data) {
			throw std::runtime_error("ASSET ERROR: Failed to load image file!");
		}

		const uint32_t bytesPerPixel = 4;

		TextureInfo textureInfo{};
		textureInfo.width = width;
		textureInfo.height = height;
		textureInfo.format = Format::R8G8B8A8_UNORM;
		textureInfo.bindFlags = BindFlag::SHADER_RESOURCE;

		SubresourceData subresourceData{};
		subresourceData.data = data;
		subresourceData.rowPitch = static_cast<uint32_t>(width) * bytesPerPixel;

		g_GfxDevice->create_texture(textureInfo, asset->texture, &subresourceData);

		stbi_image_free(data);

		outAsset.internalState = asset;
	}

	void load_from_file(Asset& outAsset, const std::string& path) {
		std::weak_ptr<AssetInternal>& weakAsset = g_Assets[path];
		std::shared_ptr<AssetInternal> asset = weakAsset.lock();

		if (asset == nullptr) { // new asset added
			asset = std::make_shared<AssetInternal>();
			g_Assets[path] = asset;
		}

		std::string extension = path.substr(path.find_last_of('.') + 1);
		const auto search = g_Types.find(extension);

		if (search == g_Types.end()) {
			throw std::runtime_error("FILE ERROR: File format not supported!");
		}

		const std::string fullPath = ENGINE_BASE_DIR + path;
		const DataType dataType = search->second;

		switch (dataType) {
		case DataType::MODEL:
			{
				load_model(outAsset, fullPath, asset);
			}
			break;
		case DataType::IMAGE:
			{
				load_texture(outAsset, fullPath, asset);
			}
			break;
		}
	}

}

const Model* Asset::get_model() const {
	return &((AssetInternal*)internalState.get())->model;
}

const Texture* Asset::get_texture() const {
	return &((AssetInternal*)internalState.get())->texture;
}
