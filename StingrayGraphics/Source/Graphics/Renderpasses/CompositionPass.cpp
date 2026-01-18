#include "CompositionPass.h"
#include "Data/ComponentTypes.h"
#include "Data/Model.h"

namespace SRCompositionPass {
	void build(SRRenderPass& self, SRGFXDevice& gfx_device, SRShaderCompiler& shader_compiler) {
		auto& passData = self.allocate_pass_data<CompositionPassData>();
		SRShaderCompiler_CompileFromFile(&shader_compiler, Str8_Literal(RES_DIR "Shaders/CompositionPass.hlsl"), { SRShaderStage::Vertex, Str8_Literal("vertexMain") }, &passData.vertexShader);
		SRShaderCompiler_CompileFromFile(&shader_compiler, Str8_Literal(RES_DIR "Shaders/CompositionPass.hlsl"), { SRShaderStage::Pixel, Str8_Literal("pixelMain") }, &passData.pixelShader);

		const SRPipelineInfo pipelineInfo = {
			.vertexShader = &passData.vertexShader,
			.pixelShader = &passData.pixelShader,
			.numRenderTargets = 1,
			.renderTargetFormats = { SRFormat::RGBA8_UNORM }
		};
		SRGFX_CreatePipeline(&gfx_device, &pipelineInfo, &passData.pipeline);
	}

	void execute(SRRenderPass& self, SRGFXDevice& gfx_device, SRCmdList cmd_list, const SRFrameInfo* frame_info) {
		auto* passData = self.get_pass_data<CompositionPassData>();

		const SRViewport viewport = {
			.width = static_cast<f32>(frame_info->width),
			.height = static_cast<f32>(frame_info->height),
		};

		const auto* gBufferAlbedo = self.get_attachment("GBufferAlbedo");
		passData->pushConstants.gBufferAlbedoIndex = SRGFX_GetDescriptorIndexSRV(&gfx_device, gBufferAlbedo->texture);

		SRGFX_BindViewport(&gfx_device, &viewport, cmd_list);
		SRGFX_BindPipeline(&gfx_device, &passData->pipeline, cmd_list);
		SRGFX_PushConstants(&gfx_device, &passData->pushConstants, sizeof(passData->pushConstants), cmd_list);
		SRGFX_Draw(&gfx_device, 3, 0, cmd_list);
	}
}
