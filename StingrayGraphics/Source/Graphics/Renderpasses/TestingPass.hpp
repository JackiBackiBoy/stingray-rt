#pragma once

#include "Graphics/GraphicsDevice.hpp"
#include "Graphics/RenderGraph.hpp"
#include "Graphics/ShaderCompiler.hpp"

namespace SRTestingPass {
	void build(SRRenderPass& self, SRGraphicsDevice& gfxDevice, SRShaderCompiler& shaderCompiler);
	void execute(SRRenderPass& self, SRGraphicsDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo);
}
