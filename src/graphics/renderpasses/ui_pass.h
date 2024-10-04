#pragma once

#include "../gfx_device.h"
#include "../render_graph.h"

#include <cstdint>
#include <vector>

class UIPass {
public:
	UIPass(GFXDevice& gfxDevice);
	~UIPass() {}

	void execute(PassExecuteInfo& executeInfo);

private:
	static constexpr uint32_t MAX_UI_PARAMS = 2048;

	struct PushConstant {
		glm::mat4 projectionMatrix;
		uint32_t uiParamsBufferIndex;
	} m_PushConstant = {};

	struct UIParams {
		glm::vec4 color = {};
		glm::vec2 position = {};
		glm::vec2 size = {};
		glm::vec2 texCoords[4] = {};
		uint32_t texIndex = 0;
		uint32_t pad1;
		uint32_t pad2;
		uint32_t pad3;
	};

	GFXDevice& m_GfxDevice;
	Shader m_VertexShader = {};
	Shader m_PixelShader = {};
	Pipeline m_Pipeline = {};
	Buffer m_UIParamsBuffers[GFXDevice::FRAMES_IN_FLIGHT] = {};
	std::vector<UIParams> m_UIParamsData = {};
};