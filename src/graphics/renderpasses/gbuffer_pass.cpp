#include "gbuffer_pass.h"

GBufferPass::GBufferPass(GFXDevice& gfxDevice) : m_GfxDevice(gfxDevice) {
	m_GfxDevice.create_shader(ShaderStage::VERTEX, "shaders/vulkan/gbuffer.vert.spv", m_VertexShader);
	m_GfxDevice.create_shader(ShaderStage::PIXEL, "shaders/vulkan/gbuffer.frag.spv", m_PixelShader);

	const PipelineInfo pipelineInfo = {
		.vertexShader = &m_VertexShader,
		.pixelShader = &m_PixelShader,
		.rasterizerState = {
			.cullMode = CullMode::BACK,
			.frontCW = true
		},
		.depthStencilState = {
			.depthEnable = true,
			.stencilEnable = false,
			.depthWriteMask = DepthWriteMask::ALL,
			.depthFunction = ComparisonFunc::LESS
		},
		.inputLayout = {
			.elements = {
				{ "POSITION", Format::R32G32B32_FLOAT },
				{ "NORMAL", Format::R32G32B32_FLOAT },
				{ "TANGENT", Format::R32G32B32_FLOAT },
				{ "TEXCOORD", Format::R32G32_FLOAT }
			}
		},
		.numRenderTargets = 3,
		.renderTargetFormats = {
			Format::R32G32B32A32_FLOAT,
			Format::R8G8B8A8_UNORM,
			Format::R16G16B16A16_FLOAT
		},
		.depthStencilFormat = Format::D32_FLOAT
	};

	m_GfxDevice.create_pipeline(pipelineInfo, m_Pipeline);
}

void GBufferPass::execute(PassExecuteInfo& executeInfo, Scene& scene) {
	const CommandList& cmdList = *executeInfo.cmdList;

	// Update
	m_PushConstant.frameIndex = m_GfxDevice.get_frame_index();

	// Rendering
	m_GfxDevice.bind_pipeline(m_Pipeline, cmdList);

	const auto& entities = scene.get_entities();
	for (const auto& entity : entities) {
		const Renderable* renderable = ecs::get_component_renderable(entity);
		const Transform* transform = ecs::get_component_transform(entity);
		const Model* model = renderable->model;

		const glm::mat4 matScale = glm::scale(glm::mat4(1.0f), transform->scale);
		const glm::mat4 matRotation = glm::mat4_cast(transform->orientation);
		const glm::mat4 matTranslation = glm::translate(glm::mat4(1.0f), transform->position);

		m_PushConstant.modelMatrix = matTranslation * matRotation * matScale;

		m_GfxDevice.bind_vertex_buffer(model->vertexBuffer, cmdList);
		m_GfxDevice.bind_index_buffer(model->indexBuffer, cmdList);

		// TODO: It would likely be better to just have a material buffer
		for (const auto& mesh : model->meshes) {
			for (const auto& primitive : mesh.primitives) {
				// Albedo map
				if (primitive.albedoMapIndex == ~0) {
					m_PushConstant.albedoMapIndex = 0;
				}
				else {
					m_PushConstant.albedoMapIndex = m_GfxDevice.get_descriptor_index(model->materialTextures[primitive.albedoMapIndex]);
				}

				// Normal map
				if (primitive.normalMapIndex == ~0) {
					m_PushConstant.normalMapIndex = 1;
				}
				else {
					m_PushConstant.normalMapIndex = m_GfxDevice.get_descriptor_index(model->materialTextures[primitive.normalMapIndex]);
				}

				m_GfxDevice.push_constants(&m_PushConstant, sizeof(m_PushConstant), cmdList);

				m_GfxDevice.draw_indexed(primitive.numIndices, primitive.baseIndex, primitive.baseVertex, cmdList);
			}
		}
	}
}
