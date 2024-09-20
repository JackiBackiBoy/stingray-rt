#include "lighting_pass.h"

LightingPass::LightingPass(GFXDevice& gfxDevice) : m_GfxDevice(gfxDevice) {
	m_GfxDevice.create_shader(ShaderStage::VERTEX, "shaders/vulkan/lighting.vert.spv", m_VertexShader);
	m_GfxDevice.create_shader(ShaderStage::PIXEL, "shaders/vulkan/lighting.frag.spv", m_PixelShader);

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

void LightingPass::execute(PassExecuteInfo& executeInfo) {
	const CommandList& cmdList = *executeInfo.cmdList;

	m_GfxDevice.bind_pipeline(m_Pipeline, cmdList);
	m_GfxDevice.draw(3, 0, cmdList);
}
