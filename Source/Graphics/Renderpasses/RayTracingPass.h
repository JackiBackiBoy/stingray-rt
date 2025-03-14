#pragma once

#include "Graphics/GraphicsDevice.h"
#include "Graphics/RenderGraph.h"
#include "Data/Scene.h"
#include "ECS/ECS.h"
#include "Managers/MaterialManager.h"

#include <vector>

namespace SR {
	class RayTracingPass {
	public:
		RayTracingPass(GraphicsDevice& gfxDevice);
		~RayTracingPass() {}

		void initialize(Scene& scene, MaterialManager& materialManager);
		void build_acceleration_structures(const CommandList& cmdList);
		void execute(PassExecuteInfo& executeInfo, Scene& scene);

		uint32_t m_RayBounces = 8;
		uint32_t m_SamplesPerPixel = 1;
		bool m_UseNormalMaps = true;
		bool m_UseSkybox = true;

	private:
		struct PushConstant {
			uint32_t frameIndex;
			uint32_t rtAccumulationIndex;
			uint32_t rtImageIndex;
			uint32_t sceneDescBufferIndex;
			uint32_t rayBounces;
			uint32_t samplesPerPixel;
			uint32_t totalSamplesPerPixel;
			uint32_t useNormalMaps;
			uint32_t useSkybox;
		} m_PushConstant = {};

		struct Object {
			uint64_t verticesBDA = 0;
			uint64_t indicesBDA = 0;
			uint64_t materialsBDA = 0;
			uint64_t matIndexOverride = 0;
		};

		GraphicsDevice& m_GfxDevice;

		RTPipeline m_RTPipeline = {};
		Shader m_RayGenShader = {};
		Shader m_MissShader = {};
		Shader m_ClosestHitShader = {};
		ShaderBindingTable m_RayGenSBT = {};
		ShaderBindingTable m_MissSBT = {};
		ShaderBindingTable m_HitSBT = {};

		std::vector<RTAS> m_BLASes = {};
		RTAS m_TLAS = {};
		std::vector<RTTLAS::BLASInstance> m_Instances = {};
		Buffer m_InstanceBuffer = {};
		Buffer m_SceneDescBuffer = {};
		std::vector<Object> m_SceneDescBufferData = {};

		uint32_t m_TotalSamplesPerPixel = m_SamplesPerPixel;
	};
}