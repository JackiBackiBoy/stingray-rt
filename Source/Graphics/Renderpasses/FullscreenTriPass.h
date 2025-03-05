#pragma once

#include "Graphics/GraphicsDevice.h"
#include "Graphics/RenderGraph.h"

class FullscreenTriPass {
public:
	FullscreenTriPass(GFXDevice& gfxDevice);
	~FullscreenTriPass() {}

	void execute(PassExecuteInfo& executeInfo);
private:
	struct PushConstant {
		uint32_t texIndex = 0;
	} m_PushConstant = {};

	GFXDevice& m_GfxDevice;
	Shader m_VertexShader = {};
	Shader m_PixelShader = {};
	Pipeline m_Pipeline = {};
};