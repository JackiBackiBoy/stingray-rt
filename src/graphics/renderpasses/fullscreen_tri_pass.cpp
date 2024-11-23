#include "fullscreen_tri_pass.h"

FullscreenTriPass::FullscreenTriPass(GFXDevice& gfxDevice) :
	m_GfxDevice(gfxDevice) {

	m_GfxDevice.create_shader(ShaderStage::VERTEX, "shaders/vulkan/fullscreen_tri.vert.spv", m_VertexShader);
	m_GfxDevice.create_shader(ShaderStage::PIXEL, "shaders/vulkan/fullscreen_tri.frag.spv", m_PixelShader);

	const PipelineInfo pipelineInfo = {
		.vertexShader = &m_VertexShader,
		.pixelShader = &m_PixelShader,
		.inputLayout = {},
		.numRenderTargets = 1,
		.renderTargetFormats = {
			Format::R8G8B8A8_UNORM
		}
	};

	m_GfxDevice.create_pipeline(pipelineInfo, m_Pipeline);
}

void FullscreenTriPass::execute(PassExecuteInfo& executeInfo) {
	const CommandList& cmdList = *executeInfo.cmdList;
	RenderGraph& renderGraph = *executeInfo.renderGraph;

	auto rtOutput = renderGraph.get_attachment("RTOutput");

	// Rendering
	m_PushConstant.texIndex = m_GfxDevice.get_descriptor_index(rtOutput->texture, SubresourceType::SRV);

	m_GfxDevice.bind_pipeline(m_Pipeline, cmdList);
	m_GfxDevice.push_constants(&m_PushConstant, sizeof(m_PushConstant), cmdList);
	m_GfxDevice.draw(3, 0, cmdList);
}
