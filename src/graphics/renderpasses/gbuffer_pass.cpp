#include "gbuffer_pass.h"

GBufferPass::GBufferPass(GFXDevice& gfxDevice) : m_GfxDevice(gfxDevice) {
	m_GfxDevice.create_shader(ShaderStage::VERTEX, "shaders/vulkan/gbuffer.vert.spv", m_VertexShader);
	m_GfxDevice.create_shader(ShaderStage::PIXEL, "shaders/vulkan/gbuffer.frag.spv", m_PixelShader);

	const PipelineInfo pipelineInfo = {
		.vertexShader = &m_VertexShader,
		.pixelShader = &m_PixelShader,
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
	};

	m_GfxDevice.create_pipeline(pipelineInfo, m_Pipeline);
}

void GBufferPass::execute(PassExecuteInfo& executeInfo, const std::vector<entity_id>& entities) {
	const CommandList& cmdList = *executeInfo.cmdList;

	// Update
	m_PushConstant.frameIndex = m_GfxDevice.get_frame_index();

	// Rendering
	m_GfxDevice.bind_pipeline(m_Pipeline, cmdList);
	m_GfxDevice.push_constants(&m_PushConstant, sizeof(m_PushConstant), cmdList);

	for (const auto& entity : entities) {
		Renderable* renderable = ecs::get_component_renderable(entity);
		const Model* model = renderable->model;

		m_GfxDevice.bind_vertex_buffer(model->vertexBuffer, cmdList);
		m_GfxDevice.bind_index_buffer(model->indexBuffer, cmdList);

		for (const auto& mesh : model->meshes) {
			m_GfxDevice.draw_indexed(mesh.numIndices, mesh.baseIndex, mesh.baseVertex, cmdList);
		}
	}
}
