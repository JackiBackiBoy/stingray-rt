#include "CompositionPass.h"
#include "Data/ComponentTypes.h"
#include "Data/Model.h"

namespace SRCompositionPass {
	void build(SRRenderPass& self, SRGFXDevice& gfxDevice, SRShaderCompiler& shaderCompiler) {
		auto& passData = self.allocate_pass_data<CompositionPassData>();
		shaderCompiler.compile_from_file(RES_DIR "Shaders/CompositionPass.hlsl", { SRShaderStage::Vertex, "vertexMain" }, passData.vertexShader);
		shaderCompiler.compile_from_file(RES_DIR "Shaders/CompositionPass.hlsl", { SRShaderStage::Pixel, "pixelMain" }, passData.pixelShader);

		const SRPipelineInfo pipelineInfo = {
			.vertexShader = &passData.vertexShader,
			.pixelShader = &passData.pixelShader,
			.numRenderTargets = 1,
			.renderTargetFormats = { SRFormat::RGBA8_UNORM }
		};
		SRGFX_CreatePipeline(&gfxDevice, &pipelineInfo, &passData.pipeline);
	}

	void execute(SRRenderPass& self, SRGFXDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo) {
		auto* passData = self.get_pass_data<CompositionPassData>();

		const SRViewport viewport = {
			.width = static_cast<float>(frameInfo.width),
			.height = static_cast<float>(frameInfo.height),
		};

		const auto* gBufferAlbedo = self.get_attachment("GBufferAlbedo");
		passData->pushConstants.gBufferAlbedoIndex = SRGFX_GetDescriptorIndexSRV(&gfxDevice, &gBufferAlbedo->texture);

		SRGFX_BindViewport(&gfxDevice, &viewport, &cmdList);
		SRGFX_BindPipeline(&gfxDevice, &passData->pipeline, &cmdList);
		SRGFX_PushConstants(&gfxDevice, &passData->pushConstants, sizeof(passData->pushConstants), &cmdList);
		SRGFX_Draw(&gfxDevice, 3, 0, &cmdList);
	}
}
