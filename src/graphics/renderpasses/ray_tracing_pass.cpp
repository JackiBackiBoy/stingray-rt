#include "ray_tracing_pass.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

RayTracingPass::RayTracingPass(GFXDevice& gfxDevice, Scene& scene) : m_GfxDevice(gfxDevice) {

	const auto& entities = scene.get_entities();

	// ----------------------------- Create BLASes -----------------------------
	size_t numBLASes = 0;
	
	// Count number of BLASes required
	for (const auto& entity : entities) {
		const Renderable* renderable = ecs::get_component_renderable(entity);
		const Model* model = renderable->model;

		for (const auto& mesh : model->meshes) {
			numBLASes += mesh.primitives.size();
		}
	}
	m_BLASes.reserve(numBLASes);
	m_Instances.reserve(numBLASes);
	m_GfxDevice.create_rt_instance_buffer(m_InstanceBuffer, numBLASes);

	// TODO: Rename MeshPrimitive to just "Mesh", GLTF terminology is confusing
	for (const auto& entity : entities) {
		const Renderable* renderable = ecs::get_component_renderable(entity);
		const Transform* transform = ecs::get_component_transform(entity);
		const Model* model = renderable->model;

		for (const auto& mesh : model->meshes) {
			for (const auto& primitive : mesh.primitives) {
				RTAS& blas = m_BLASes.emplace_back();

				const RTASInfo blasInfo = {
					.type = RTASType::BLAS,
					.blas = {
						.geometries = {
							RTBLASGeometry {
								.type = RTBLASGeometry::Type::TRIANGLES,
								.triangles = {
									.vertexBuffer = &model->vertexBuffer,
									.indexBuffer = &model->indexBuffer,
									.vertexFormat = Format::R32G32B32_FLOAT,
									.vertexCount = primitive.numVertices,
									.vertexStride = sizeof(ModelVertex),
									.vertexByteOffset = sizeof(ModelVertex) * primitive.baseVertex,
									.indexCount = primitive.numIndices,
									.indexOffset = primitive.baseIndex
								}
							}
						}
					}
				};

				m_GfxDevice.create_rtas(blasInfo, blas);

				// Create BLAS instance data
				RTTLAS::BLASInstance instance = {
					.instanceID = static_cast<uint32_t>(m_Instances.size()),
					.instanceMask = 1,
					.instanceContributionHitGroupIndex = 0, // TODO: Check out
					.blasResource = &blas
				};

				// Update the transform data
				glm::mat4 scale = glm::scale(glm::mat4(1.0f), transform->scale);
				glm::mat4 rotation = glm::mat4_cast(transform->orientation);
				glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform->position);
				glm::mat4 transformation = glm::transpose(translation * rotation * scale); // TODO: Might be wrong

				std::memcpy(instance.transform, &transformation[0][0], sizeof(instance.transform));

				m_Instances.push_back(instance);
			}
		}
	}

	// ------------------------------ Create TLAS ------------------------------
	const RTASInfo tlasInfo = {
		.type = RTASType::TLAS,
		.tlas = {
			.instanceBuffer = &m_InstanceBuffer,
			.numInstances = static_cast<uint32_t>(numBLASes)
		}
	};

	m_GfxDevice.create_rtas(tlasInfo, m_TLAS);

	// ---------------------------- Write Instances ----------------------------
	for (size_t i = 0; i < m_Instances.size(); ++i) {
		void* dataSection = (uint8_t*)m_InstanceBuffer.mappedData + i * m_InstanceBuffer.info.stride;
		m_GfxDevice.write_blas_instance(m_Instances[i], dataSection);
	}

	// ---------------------------- Shader Binding Table ----------------------------
}

void RayTracingPass::execute(PassExecuteInfo& executeInfo, Scene& scene) {

}
