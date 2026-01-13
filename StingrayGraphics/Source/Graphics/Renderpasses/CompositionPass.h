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
	void build(SRRenderPass& self, SRGFXDevice& gfx_device, SRShaderCompiler& shader_compiler);
	void execute(SRRenderPass& self, SRGFXDevice& gfx_device, SRCmdList cmd_list, const SRFrameInfo* frame_info);
}
