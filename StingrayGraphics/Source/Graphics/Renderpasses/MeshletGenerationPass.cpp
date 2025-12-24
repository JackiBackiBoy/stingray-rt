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

	void build(SRRenderPass& self, SRGraphicsDevice& gfxDevice, SRShaderCompiler& shaderCompiler) {
		auto& passData = self.allocate_pass_data<MeshletGenerationPassData>();
		shaderCompiler.compile_from_file(RES_DIR "Shaders/MeshShader.hlsl", { SRShaderStage::Mesh, "meshMain" }, passData.meshShader);
		shaderCompiler.compile_from_file(RES_DIR "Shaders/MeshShader.hlsl", { SRShaderStage::Pixel, "pixelMain" }, passData.pixelShader);

		SRPipelineInfo pipelineInfo = {
			.pixelShader = &passData.pixelShader,
			.meshShader = &passData.meshShader,
			.rasterizerState = {
				.cullMode = SRCullMode::Back,
			},
			.numRenderTargets = 1,
			.renderTargetFormats = { SRFormat::RGBA8_UNORM }
		};
		gfxDevice.create_pipeline(pipelineInfo, passData.pipeline);
	}

	void execute(SRRenderPass& self, SRGraphicsDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo) {
		auto* passData = self.get_pass_data<MeshletGenerationPassData>();

		SRViewport viewport = {
			.width = static_cast<f32>(frameInfo.width),
			.height = static_cast<f32>(frameInfo.height),
		};

		gfxDevice.bind_viewport(viewport, cmdList);
		gfxDevice.bind_pipeline(passData->pipeline, cmdList);
		gfxDevice.bind_root_constant_buffer(*frameInfo.perFrameBuffer, cmdList);

		frameInfo.scene->for_each<SRTransform, SRRenderable>([&](SRTransform& t, SRRenderable& r) {
			const SRModel* model = r.model;
			u32 numMeshlets = model->numMeshlets;

			passData->push.vertexBufferIdx = gfxDevice.get_descriptor_index_srv(model->vertexBuffer);
			passData->push.meshletBufferIdx = gfxDevice.get_descriptor_index_srv(model->meshletBuffer);
			passData->push.meshletVerticesBufferIdx = gfxDevice.get_descriptor_index_srv(model->meshletVerticesBuffer);
			passData->push.meshletTrianglesBufferIdx = gfxDevice.get_descriptor_index_srv(model->meshletTrianglesBuffer);

			gfxDevice.push_constants(&passData->push, sizeof(passData->push), cmdList);
			gfxDevice.dispatch_mesh(numMeshlets, 1, 1, cmdList);
		});
	}

}
