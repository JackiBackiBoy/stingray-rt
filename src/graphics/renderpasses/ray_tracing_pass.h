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

	void build_acceleration_structures(const CommandList& cmdList);
	void execute(PassExecuteInfo& executeInfo, Scene& scene);

private:
	GFXDevice& m_GfxDevice;

	RTPipeline m_RTPipeline = {};
	Shader m_RayGenShader = {};
	Shader m_MissShader = {};
	Shader m_ClosestHitShader = {};

	std::vector<RTAS> m_BLASes = {}; // NOTE: One per mesh
	RTAS m_TLAS = {};
	std::vector<RTTLAS::BLASInstance> m_Instances = {};
	Buffer m_InstanceBuffer = {};
};