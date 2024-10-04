#include "ui_pass.h"

UIPass::UIPass(GFXDevice& gfxDevice) : m_GfxDevice(gfxDevice) {
	m_GfxDevice.create_shader(ShaderStage::VERTEX, "shaders/vulkan/ui.vert.spv", m_VertexShader);
	m_GfxDevice.create_shader(ShaderStage::PIXEL, "shaders/vulkan/ui.frag.spv", m_PixelShader);

	const PipelineInfo pipelineInfo = {
		.vertexShader = &m_VertexShader,
		.pixelShader = &m_PixelShader,
		.numRenderTargets = 1,
		.renderTargetFormats = { Format::R8G8B8A8_UNORM }
	};

	m_GfxDevice.create_pipeline(pipelineInfo, m_Pipeline);

	const BufferInfo uiParamsBufferInfo = {
		.size = MAX_UI_PARAMS * sizeof(UIParams),
		.stride = sizeof(UIParams),
		.usage = Usage::UPLOAD,
		.bindFlags = BindFlag::SHADER_RESOURCE,
		.miscFlags = MiscFlag::BUFFER_STRUCTURED,
		.persistentMap = true
	};

	// TODO: Temporary
	m_UIParamsData.push_back(UIParams{
		.position = { 0.0f, 0.0f },
		.size = { 200, 100 }
	});
	//

	for (size_t i = 0; i < GFXDevice::FRAMES_IN_FLIGHT; ++i) {
		m_GfxDevice.create_buffer(uiParamsBufferInfo, m_UIParamsBuffers[i], m_UIParamsData.data());
	}

	
}

void UIPass::execute(PassExecuteInfo& executeInfo) {
	const CommandList& cmdList = *executeInfo.cmdList;
	const FrameInfo& frameInfo = *executeInfo.frameInfo;

	m_PushConstant.projectionMatrix = glm::ortho(0.0f, (float)frameInfo.width, (float)frameInfo.height, 0.0f);
	m_PushConstant.uiParamsBufferIndex = m_GfxDevice.get_descriptor_index(m_UIParamsBuffers[m_GfxDevice.get_frame_index()]);

	m_GfxDevice.bind_pipeline(m_Pipeline, cmdList);
	m_GfxDevice.push_constants(&m_PushConstant, sizeof(m_PushConstant), cmdList);

	m_GfxDevice.draw_instanced(6, 1, 0, 0, cmdList);
}
