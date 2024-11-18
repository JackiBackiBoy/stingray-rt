#pragma once

#include "../gfx_device.h"
#include "../render_graph.h"
#include "../../data/scene.h"
#include "../../ecs/ecs.h"

#include <vector>

class RayTracingPass {
public:
	RayTracingPass(GFXDevice& gfxDevice, Scene& scene);
	~RayTracingPass() {}

	void execute(PassExecuteInfo& executeInfo, Scene& scene);
private:
	GFXDevice& m_GfxDevice;

	std::vector<RTAS> m_BLASes = {}; // NOTE: One per mesh
	RTAS m_TLAS = {};
	std::vector<RTTLAS::BLASInstance> m_Instances = {};
	Buffer m_InstanceBuffer = {};
};