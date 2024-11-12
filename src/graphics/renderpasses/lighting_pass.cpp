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

	// Lighting buffers
	const BufferInfo lightingUBOInfo = {
		.size = sizeof(LightingUBO),
		.stride = sizeof(LightingUBO),
		.usage = Usage::UPLOAD,
		.bindFlags = BindFlag::UNIFORM_BUFFER,
		.persistentMap = true
	};

	for (uint32_t i = 0; i < GFXDevice::FRAMES_IN_FLIGHT; ++i) {
		m_GfxDevice.create_buffer(lightingUBOInfo, m_LightingUBOs[i], &m_LightingUBOData);
	}
}

void LightingPass::execute(PassExecuteInfo& executeInfo, Scene& scene) {
	const CommandList& cmdList = *executeInfo.cmdList;
	RenderGraph& renderGraph = *executeInfo.renderGraph;

	auto positionAttachment = renderGraph.get_attachment("Position");
	auto albedoAttachment = renderGraph.get_attachment("Albedo");
	auto normalAttachment = renderGraph.get_attachment("Normal");

	// Lighting data
	m_LightingUBOData.directionLight.direction = scene.get_sun_direction();
	memcpy(m_LightingUBOs[m_GfxDevice.get_frame_index()].mappedData, &m_LightingUBOData, sizeof(m_LightingUBOData));

	// Rendering
	m_PushConstant.positionIndex = m_GfxDevice.get_descriptor_index(positionAttachment->texture);
	m_PushConstant.albedoIndex = m_GfxDevice.get_descriptor_index(albedoAttachment->texture);
	m_PushConstant.normalIndex = m_GfxDevice.get_descriptor_index(normalAttachment->texture);
	m_PushConstant.lightingUBOIndex = m_GfxDevice.get_descriptor_index(m_LightingUBOs[m_GfxDevice.get_frame_index()]);

	m_GfxDevice.bind_pipeline(m_Pipeline, cmdList);
	m_GfxDevice.push_constants(&m_PushConstant, sizeof(m_PushConstant), cmdList);
	m_GfxDevice.draw(3, 0, cmdList);
}
