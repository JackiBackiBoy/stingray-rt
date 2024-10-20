#pragma once

#include "../gfx_device.h"
#include "../render_graph.h"
#include "../../ecs/ecs.h"

class GBufferPass {
public:
	GBufferPass(GFXDevice& gfxDevice);
	~GBufferPass() {}

	void execute(PassExecuteInfo& executeInfo, const std::vector<entity_id>& entities); // TEMP

private:
	struct PushConstant {
		glm::mat4 modelMatrix = { 1.0f };
		uint32_t frameIndex = 0;
		uint32_t albedoMapIndex = 0;
		uint32_t normalMapIndex = 0;
	};

	GFXDevice& m_GfxDevice;
	Shader m_VertexShader = {};
	Shader m_PixelShader = {};
	Pipeline m_Pipeline = {};
	PushConstant m_PushConstant = {};
};