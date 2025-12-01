#include "CompositionPass.hpp"
#include "Data/ComponentTypes.hpp"
#include "Data/Model.hpp"

namespace SRCompositionPass {
	struct CompositionPassData {
		SRPipeline pipeline;
		SRShader vertexShader;
		SRShader pixelShader;

		struct PushConstants {
			SRDescriptorIndex gBufferAlbedoIndex;
		} pushConstants;
	};

	void build(SRRenderPass& self, SRGraphicsDevice& gfxDevice, SRShaderCompiler& shaderCompiler) {
		auto& passData = self.allocate_pass_data<CompositionPassData>();
		shaderCompiler.compile_from_file(RES_DIR "Shaders/CompositionPass.slang", { SRShaderStage::Vertex, "vertexMain" }, passData.vertexShader);
		shaderCompiler.compile_from_file(RES_DIR "Shaders/CompositionPass.slang", { SRShaderStage::Pixel, "pixelMain" }, passData.pixelShader);

		const SRPipelineInfo pipelineInfo = {
			.vertexShader = &passData.vertexShader,
			.pixelShader = &passData.pixelShader,
			.numRenderTargets = 1,
			.renderTargetFormats = { SRFormat::RGBA8_UNORM }
		};
		gfxDevice.create_pipeline(pipelineInfo, passData.pipeline);
	}

	void execute(SRRenderPass& self, SRGraphicsDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo) {
		auto* passData = self.get_pass_data<CompositionPassData>();

		const SRViewport viewport = {
			.width = static_cast<float>(frameInfo.width),
			.height = static_cast<float>(frameInfo.height),
		};

		const auto* gBufferAlbedo = self.get_attachment("GBufferAlbedo");
		passData->pushConstants.gBufferAlbedoIndex = gfxDevice.get_descriptor_index_srv(gBufferAlbedo->texture);

		gfxDevice.bind_viewport(viewport, cmdList);
		gfxDevice.bind_pipeline(passData->pipeline, cmdList);
		gfxDevice.push_constants(&passData->pushConstants, sizeof(passData->pushConstants), cmdList);
		gfxDevice.draw(3, 0, cmdList);
	}
}
