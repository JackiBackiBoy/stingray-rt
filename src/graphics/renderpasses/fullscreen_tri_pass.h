#pragma once

#include "../gfx_device.h"
#include "../render_graph.h"

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