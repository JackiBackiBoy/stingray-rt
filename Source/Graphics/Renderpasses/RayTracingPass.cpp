#include "RayTracingPass.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace SR {
	RayTracingPass::RayTracingPass(GraphicsDevice& gfxDevice) : m_GfxDevice(gfxDevice) {
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
	}

	void RayTracingPass::initialize(Scene& scene, MaterialManager& materialManager) {
		const auto& entities = scene.get_entities();
		const Buffer& materialBuffer = materialManager.get_material_buffer();

		// ----------------------------- Create BLASes -----------------------------
		size_t numBLASes = 0;

		// Count number of BLASes required
		for (const auto& entity : entities) {
			const Renderable* renderable = ECS::get_component<Renderable>(entity);
			const Model* model = renderable->model;

			for (const auto& mesh : model->meshes) {
				numBLASes += mesh.primitives.size();
			}
		}

		m_BLASes.reserve(numBLASes);
		m_Instances.reserve(numBLASes);
		m_SceneDescBufferData.reserve(numBLASes);
		m_GfxDevice.create_rt_instance_buffer(m_InstanceBuffer, static_cast<uint32_t>(numBLASes));

		// TODO: Rename MeshPrimitive to just "Mesh", GLTF terminology is confusing
		for (const auto& entity : entities) {
			const Renderable* renderable = ECS::get_component<Renderable>(entity);
			const Transform* transform = ECS::get_component<Transform>(entity);
			const Material* material = ECS::get_component<Material>(entity);
			const Model* model = renderable->model;

			uint32_t matIndexOverride = material != nullptr ? materialManager.add_material(*material) : 0;

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
										.vertexCount = primitive.numVertices,
										.vertexStride = sizeof(ModelVertex),
										.vertexByteOffset = sizeof(ModelVertex) * primitive.baseVertex,
										.indexCount = primitive.numIndices,
										.indexOffset = primitive.baseIndex,
										.vertexFormat = Format::R32G32B32_FLOAT
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
						// That is at least what seems to be the case, so this might not be true
						.blasResource = &blas
					};

					// Update the transform data
					glm::mat4 scale = glm::scale(glm::mat4(1.0f), transform->scale);
					glm::mat4 rotation = glm::mat4_cast(transform->orientation);
					glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform->position);
					glm::mat4 transformation = glm::transpose(translation * rotation * scale); // TODO: Might be wrong

					std::memcpy(instance.transform, &transformation[0][0], sizeof(instance.transform));

					m_Instances.push_back(instance);

					// Create object for scene desc
					Object& object = m_SceneDescBufferData.emplace_back();
					object.verticesBDA = m_GfxDevice.get_bda(model->vertexBuffer) + primitive.baseVertex * sizeof(ModelVertex);
					object.indicesBDA = m_GfxDevice.get_bda(model->indexBuffer) + primitive.baseIndex * sizeof(uint32_t);
					object.materialsBDA = m_GfxDevice.get_bda(materialBuffer);
					object.matIndexOverride = matIndexOverride;
				}
			}
		}

		materialManager.update_gpu_buffer();


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

		// ------------------------- Shader Binding Tables -------------------------
		m_GfxDevice.create_shader_binding_table(m_RTPipeline, 0, m_RayGenSBT);
		m_GfxDevice.create_shader_binding_table(m_RTPipeline, 1, m_MissSBT);
		m_GfxDevice.create_shader_binding_table(m_RTPipeline, 2, m_HitSBT);

		// --------------------------- Create Scene Desc ---------------------------
		const BufferInfo sceneDescBufferInfo = {
			.size = static_cast<uint64_t>(m_Instances.size()) * sizeof(Object),
			.stride = sizeof(Object),
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::SHADER_RESOURCE,
			.miscFlags = MiscFlag::BUFFER_STRUCTURED,
			.persistentMap = false
		};

		m_GfxDevice.create_buffer(sceneDescBufferInfo, m_SceneDescBuffer, m_SceneDescBufferData.data());
	}

	void RayTracingPass::build_acceleration_structures(const CommandList& cmdList) {
		for (size_t i = 0; i < m_BLASes.size(); ++i) {
			m_GfxDevice.build_rtas(m_BLASes[i], cmdList);
		}

		m_GfxDevice.build_rtas(m_TLAS, cmdList);
	}

	void RayTracingPass::execute(PassExecuteInfo& executeInfo, Scene& scene) {
		static glm::mat4 lastViewMatrix = executeInfo.frameInfo->camera->get_view_matrix();
		static glm::mat4 lastProjMatrix = executeInfo.frameInfo->camera->get_proj_matrix();

		// Reset accumulation if camera has moved
		if (executeInfo.frameInfo->camera->get_view_matrix() != lastViewMatrix ||
			executeInfo.frameInfo->camera->get_proj_matrix() != lastProjMatrix) {
			m_TotalSamplesPerPixel = m_SamplesPerPixel; // reset accumulation
		}

		const CommandList& cmdList = *executeInfo.cmdList;
		RenderGraph& renderGraph = *executeInfo.renderGraph;

		auto rtOutput = renderGraph.get_attachment("RTOutput");
		auto rtAccumulation = renderGraph.get_attachment("RTAccumulation");

		m_PushConstant.frameIndex = m_GfxDevice.get_frame_index();
		m_PushConstant.rtAccumulationIndex = m_GfxDevice.get_descriptor_index(rtAccumulation->texture, SubresourceType::UAV);
		m_PushConstant.rtImageIndex = m_GfxDevice.get_descriptor_index(rtOutput->texture, SubresourceType::UAV);
		m_PushConstant.sceneDescBufferIndex = m_GfxDevice.get_descriptor_index(m_SceneDescBuffer, SubresourceType::SRV);
		m_PushConstant.rayBounces = m_RayBounces;
		m_PushConstant.samplesPerPixel = m_SamplesPerPixel;
		m_PushConstant.totalSamplesPerPixel = m_TotalSamplesPerPixel;
		m_PushConstant.useNormalMaps = m_UseNormalMaps ? 1 : 0;
		m_PushConstant.useSkybox = m_UseSkybox ? 1 : 0;

		m_GfxDevice.bind_rt_pipeline(m_RTPipeline, cmdList);
		m_GfxDevice.push_rt_constants(&m_PushConstant, sizeof(m_PushConstant), m_RTPipeline, cmdList);

		const DispatchRaysInfo dispatchInfo = {
			.rayGenTable = &m_RayGenSBT,
			.missTable = &m_MissSBT,
			.hitGroupTable = &m_HitSBT,
			.width = rtOutput->texture.info.width,
			.height = rtOutput->texture.info.height
		};

		m_GfxDevice.dispatch_rays(dispatchInfo, cmdList);

		m_TotalSamplesPerPixel += m_SamplesPerPixel;

		lastViewMatrix = executeInfo.frameInfo->camera->get_view_matrix();
		lastProjMatrix = executeInfo.frameInfo->camera->get_proj_matrix();
	}
}
