#pragma once

#include "../gfx_device.h"
#include "../render_graph.h"
#include "../../ecs/ecs.h"

class LightingPass {
public:
	LightingPass(GFXDevice& gfxDevice);
	~LightingPass() {}

	void execute(PassExecuteInfo& executeInfo);

private:
	struct PushConstant {
		uint32_t positionIndex;
		uint32_t albedoIndex;
		uint32_t normalIndex;
	} m_PushConstant = {};

	GFXDevice& m_GfxDevice;
	Shader m_VertexShader = {};
	Shader m_PixelShader = {};
	Pipeline m_Pipeline = {};
};