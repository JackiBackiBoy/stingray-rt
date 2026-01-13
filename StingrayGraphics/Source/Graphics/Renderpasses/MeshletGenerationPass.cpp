#include "MeshletGenerationPass.h"
#include "Data/ComponentTypes.h"
#include "Data/Model.h"

namespace SRMeshletGenerationpass {
	struct MeshletGenerationPassData {
		SRShader meshShader;
		SRShader pixelShader;
		SRPipeline pipeline;

		struct PushConstants {
			SRDescriptorIndex vertexBufferIdx;
			SRDescriptorIndex meshletBufferIdx;
			SRDescriptorIndex meshletVerticesBufferIdx;
			SRDescriptorIndex meshletTrianglesBufferIdx;
		} push;
	};

	void build(SRRenderPass& self, SRGFXDevice& gfx_device, SRShaderCompiler& shader_compiler) {
		auto& passData = self.allocate_pass_data<MeshletGenerationPassData>();
		SRShaderCompiler_CompileFromFile(&shader_compiler, RES_DIR "Shaders/MeshShader.hlsl", { SRShaderStage::Mesh, "meshMain" }, &passData.meshShader);
		SRShaderCompiler_CompileFromFile(&shader_compiler, RES_DIR "Shaders/MeshShader.hlsl", { SRShaderStage::Pixel, "pixelMain" }, &passData.pixelShader);

		SRPipelineInfo pipelineInfo = {
			.pixelShader = &passData.pixelShader,
			.meshShader = &passData.meshShader,
			.rasterizerState = {
				.cullMode = SRCullMode::Back,
			},
			.numRenderTargets = 1,
			.renderTargetFormats = { SRFormat::RGBA8_UNORM }
		};
		SRGFX_CreatePipeline(&gfx_device, &pipelineInfo, &passData.pipeline);
	}

	void execute(SRRenderPass& self, SRGFXDevice& gfx_device, const SRCmdList& cmd_list, const SRFrameInfo& frame_info) {
		auto* passData = self.get_pass_data<MeshletGenerationPassData>();

		SRViewport viewport = {
			.width = static_cast<f32>(frame_info.width),
			.height = static_cast<f32>(frame_info.height),
		};

		SRGFX_BindViewport(&gfx_device, &viewport, cmd_list);
		SRGFX_BindPipeline(&gfx_device, &passData->pipeline, cmd_list);
		SRGFX_BindRootConstantBuffer(&gfx_device, frame_info.perFrameBuffer, cmd_list);

		frame_info.scene->for_each<SRTransform, SRRenderable>([&](SRTransform& t, SRRenderable& r) {
			const SRModel* model = r.model;
			u32 numMeshlets = model->numMeshlets;

			passData->push.vertexBufferIdx = SRGFX_GetDescriptorIndexSRV(&gfx_device, model->vertexBuffer);
			passData->push.meshletBufferIdx = SRGFX_GetDescriptorIndexSRV(&gfx_device, model->meshletBuffer);
			passData->push.meshletVerticesBufferIdx = SRGFX_GetDescriptorIndexSRV(&gfx_device, model->meshletVerticesBuffer);
			passData->push.meshletTrianglesBufferIdx = SRGFX_GetDescriptorIndexSRV(&gfx_device, model->meshletTrianglesBuffer);

			SRGFX_PushConstants(&gfx_device, &passData->push, sizeof(passData->push), cmd_list);
			SRGFX_DispatchMesh(&gfx_device, numMeshlets, 1, 1, cmd_list);
		});
	}

}
