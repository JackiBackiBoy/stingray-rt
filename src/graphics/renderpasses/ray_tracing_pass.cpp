#include "ray_tracing_pass.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

RayTracingPass::RayTracingPass(GFXDevice& gfxDevice, Scene& scene) : m_GfxDevice(gfxDevice) {

	const auto& entities = scene.get_entities();

	// ---------------------- Create Ray-Tracing Pipeline ----------------------
	m_GfxDevice.create_shader(ShaderStage::RAYGEN, "shaders/vulkan/rt_raygen.rgen.spv", m_RayGenShader);
	m_GfxDevice.create_shader(ShaderStage::MISS, "shaders/vulkan/rt_miss.rmiss.spv", m_MissShader);
	m_GfxDevice.create_shader(ShaderStage::CLOSEST_HIT, "shaders/vulkan/rt_closest_hit.rchit.spv", m_ClosestHitShader);

	const RTPipelineInfo rtPipelineInfo = {
		.rayGenShader = &m_RayGenShader,
		.missShader = &m_MissShader,
		.closestHitShader = &m_ClosestHitShader,
		.shaderGroups = {
			RTShaderGroup { RTShaderGroup::Type::GENERAL,    0u, ~0u }, // ray-gen
			RTShaderGroup { RTShaderGroup::Type::GENERAL,	 1u, ~0u }, // miss
			RTShaderGroup { RTShaderGroup::Type::TRIANGLES, ~0u,  2u } // closest_hit
		},
		.payloadSize = 4 * sizeof(float)
	};

	m_GfxDevice.create_rt_pipeline(rtPipelineInfo, m_RTPipeline);

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
					// TODO (REVISIT): Ok, so it SEEMS like this is the SBT index for HIT GROUPS ONLY.
					// So, for example, if we have the following shader groups:
					// 0 -> ray-gen shader -> GENERAL
					// 1 -> miss shader -> GENERAL
					// 2 -> closest-hit -> TRIANGLES_HIT_GROUP
					// Then the closest-hit shader at index 2 in the SBT will be the only
					// hit group, and thus its index within the HIT GROUP SBT will be 0.
					// That is at least what seems to be the case, so we'll have to revisit this again
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
}

void RayTracingPass::build_acceleration_structures(const CommandList& cmdList) {
	for (size_t i = 0; i < m_BLASes.size(); ++i) {
		m_GfxDevice.build_rtas(m_BLASes[i], cmdList);
	}

	m_GfxDevice.build_rtas(m_TLAS, cmdList);
}

void RayTracingPass::execute(PassExecuteInfo& executeInfo, Scene& scene) {
}
