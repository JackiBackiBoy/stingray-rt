#pragma once

#include "Graphics/GraphicsDevice.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/ShaderCompiler.h"

namespace SRMeshletGenerationpass {
	void build(SRRenderPass& self, SRGFXDevice& gfx_device, SRShaderCompiler& shader_compiler);
	void execute(SRRenderPass& self, SRGFXDevice& gfx_device, const SRCmdList& cmd_list, const SRFrameInfo& frame_info);
}
