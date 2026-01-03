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

	void build(SRRenderPass& self, SRGFXDevice& gfxDevice, SRShaderCompiler& shaderCompiler) {
		auto& passData = self.allocate_pass_data<MeshletGenerationPassData>();
		SRShaderCompiler_CompileFromFile(&shaderCompiler, RES_DIR "Shaders/MeshShader.hlsl", { SRShaderStage::Mesh, "meshMain" }, &passData.meshShader);
		SRShaderCompiler_CompileFromFile(&shaderCompiler, RES_DIR "Shaders/MeshShader.hlsl", { SRShaderStage::Pixel, "pixelMain" }, &passData.pixelShader);

		SRPipelineInfo pipelineInfo = {
			.pixelShader = &passData.pixelShader,
			.meshShader = &passData.meshShader,
			.rasterizerState = {
				.cullMode = SRCullMode::Back,
			},
			.numRenderTargets = 1,
			.renderTargetFormats = { SRFormat::RGBA8_UNORM }
		};
		SRGFX_CreatePipeline(&gfxDevice, &pipelineInfo, &passData.pipeline);
	}

	void execute(SRRenderPass& self, SRGFXDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo) {
		auto* passData = self.get_pass_data<MeshletGenerationPassData>();

		SRViewport viewport = {
			.width = static_cast<f32>(frameInfo.width),
			.height = static_cast<f32>(frameInfo.height),
		};

		SRGFX_BindViewport(&gfxDevice, &viewport, &cmdList);
		SRGFX_BindPipeline(&gfxDevice, &passData->pipeline, &cmdList);
		SRGFX_BindRootConstantBuffer(&gfxDevice, frameInfo.perFrameBuffer, &cmdList);

		frameInfo.scene->for_each<SRTransform, SRRenderable>([&](SRTransform& t, SRRenderable& r) {
			const SRModel* model = r.model;
			u32 numMeshlets = model->numMeshlets;

			passData->push.vertexBufferIdx = SRGFX_GetDescriptorIndexSRV(&gfxDevice, &model->vertexBuffer);
			passData->push.meshletBufferIdx = SRGFX_GetDescriptorIndexSRV(&gfxDevice, &model->meshletBuffer);
			passData->push.meshletVerticesBufferIdx = SRGFX_GetDescriptorIndexSRV(&gfxDevice, &model->meshletVerticesBuffer);
			passData->push.meshletTrianglesBufferIdx = SRGFX_GetDescriptorIndexSRV(&gfxDevice, &model->meshletTrianglesBuffer);

			SRGFX_PushConstants(&gfxDevice, &passData->push, sizeof(passData->push), &cmdList);
			SRGFX_DispatchMesh(&gfxDevice, numMeshlets, 1, 1, &cmdList);
		});
	}

}
