#pragma once

#include "Graphics/GraphicsDevice.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/ShaderCompiler.h"

struct CompositionPassData {
	SRPipeline pipeline;
	SRShader vertexShader;
	SRShader pixelShader;

	struct PushConstants {
		SRDescriptorIndex gBufferAlbedoIndex;
	} pushConstants;
};

namespace SRCompositionPass {
	void build(SRRenderPass& self, SRGFXDevice& gfxDevice, SRShaderCompiler& shaderCompiler);
	void execute(SRRenderPass& self, SRGFXDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo);
}
