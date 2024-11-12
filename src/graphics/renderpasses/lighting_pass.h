#pragma once

#include "../gfx_device.h"
#include "../render_graph.h"
#include "../../data/scene.h"
#include "../../ecs/ecs.h"

class LightingPass {
public:
	LightingPass(GFXDevice& gfxDevice);
	~LightingPass() {}

	void execute(PassExecuteInfo& executeInfo, Scene& scene);

private:
	static constexpr int MAX_CASCADES = 4;

	struct DirectionLight {
		glm::mat4 cascadeProjections[MAX_CASCADES] = {};
		glm::mat4 viewMatrix = { 1.0f };
		glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // NOTE: w is intensity
		glm::vec3 direction = {};
		float cascadeDistances[MAX_CASCADES] = {};
		uint32_t numCascades = 1;
	};

	struct LightingUBO {
		DirectionLight directionLight = {};
		uint32_t numPointLights = 0;
		uint32_t pad1 = 0;
		uint32_t pad2 = 0;
		uint32_t pad3 = 0;
		PointLight pointLights[Scene::MAX_POINT_LIGHTS] = {};
	};

	struct PushConstant {
		uint32_t positionIndex;
		uint32_t albedoIndex;
		uint32_t normalIndex;
		uint32_t lightingUBOIndex;
	} m_PushConstant = {};

	GFXDevice& m_GfxDevice;
	Shader m_VertexShader = {};
	Shader m_PixelShader = {};
	Pipeline m_Pipeline = {};

	Buffer m_LightingUBOs[GFXDevice::FRAMES_IN_FLIGHT] = {};
	LightingUBO m_LightingUBOData = {};
};