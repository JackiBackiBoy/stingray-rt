#pragma once

#include "../gfx_device.h"
#include "../render_graph.h"
#include "../../data/scene.h"
#include "../../ecs/ecs.h"

#include <vector>

class RayTracingPass {
public:
	RayTracingPass(GFXDevice& gfxDevice);
	~RayTracingPass() {}

	void initialize(Scene& scene, const Buffer& materialBuffer);
	void build_acceleration_structures(const CommandList& cmdList);
	void execute(PassExecuteInfo& executeInfo, Scene& scene);

private:
	struct PushConstant {
		uint32_t frameIndex;
		uint32_t rtAccumulationIndex;
		uint32_t rtImageIndex;
		uint32_t sceneDescBufferIndex;
		uint32_t samplesPerPixel;
		uint32_t totalSamplesPerPixel;
	} m_PushConstant = {};

	struct Object {
		uint64_t verticesBDA = 0;
		uint64_t indicesBDA = 0;
		uint64_t materialsBDA = 0;
		uint64_t pad1 = 0;
	};

	GFXDevice& m_GfxDevice;

	RTPipeline m_RTPipeline = {};
	Shader m_RayGenShader = {};
	Shader m_MissShader = {};
	Shader m_ClosestHitShader = {};
	ShaderBindingTable m_RayGenSBT = {};
	ShaderBindingTable m_MissSBT = {};
	ShaderBindingTable m_HitSBT = {};

	std::vector<RTAS> m_BLASes = {}; // NOTE: One per mesh
	RTAS m_TLAS = {};
	std::vector<RTTLAS::BLASInstance> m_Instances = {};
	Buffer m_InstanceBuffer = {};

	// TODO: Might have to have a frames-in-flight amount of these buffers
	Buffer m_SceneDescBuffer = {};
	std::vector<Object> m_SceneDescBufferData = {};

	uint32_t m_SamplesPerPixel = 1;
	uint32_t m_TotalSamplesPerPixel = m_SamplesPerPixel;
};