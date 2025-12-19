#pragma once

#include "Graphics/GraphicsDevice.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/ShaderCompiler.h"

namespace SRMeshletGenerationpass {
	void build(SRRenderPass& self, SRGraphicsDevice& gfxDevice, SRShaderCompiler& shaderCompiler);
	void execute(SRRenderPass& self, SRGraphicsDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo);
}
