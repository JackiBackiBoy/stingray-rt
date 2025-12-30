#pragma once

#include "Graphics/GraphicsDevice.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/ShaderCompiler.h"

struct DepthPrepassData {
	SRPipeline pipeline;
	SRShader vertexShader;
};

namespace SRDepthPrepass {
	void build(SRRenderPass& self, SRGFXDevice& gfxDevice, SRShaderCompiler& shaderCompiler);
	void execute(SRRenderPass& self, SRGFXDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo);
}
