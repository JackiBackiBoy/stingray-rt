#include "MeshletGenerationPass.h"
#include "Data/ComponentTypes.h"
#include "Data/Model.h"

namespace SRMeshletGenerationpass {
	struct MeshletGenerationPassData {
		SRShader meshShader;
		SRShader pixelShader;
		SRPipeline pipeline;
	};

	void build(SRRenderPass& self, SRGraphicsDevice& gfxDevice, SRShaderCompiler& shaderCompiler) {
		auto& passData = self.allocate_pass_data<MeshletGenerationPassData>();
		shaderCompiler.compile_from_file(RES_DIR "Shaders/MeshShader.slang", { SRShaderStage::Mesh, "meshMain" }, passData.meshShader);
		//shaderCompiler.compile_from_file(RES_DIR "Shaders/MeshShader.slang", { SRShaderStage::Pixel, "pixelMain" }, passData.meshShader);

		const SRPipelineInfo pipelineInfo = {
			.meshShader = &passData.meshShader,
			.numRenderTargets = 1,
			.renderTargetFormats = { SRFormat::RGBA8_UNORM }
		};
		gfxDevice.create_pipeline(pipelineInfo, passData.pipeline);
	}

	void execute(SRRenderPass& self, SRGraphicsDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo) {
		auto* passData = self.get_pass_data<MeshletGenerationPassData>();

		const SRViewport viewport = {
			.width = static_cast<float>(frameInfo.width),
			.height = static_cast<float>(frameInfo.height),
		};

		gfxDevice.bind_viewport(viewport, cmdList);
		gfxDevice.bind_pipeline(passData->pipeline, cmdList);
		gfxDevice.bind_root_constant_buffer(*frameInfo.perFrameBuffer, cmdList);

		frameInfo.scene->for_each<SRTransform, SRRenderable>([&](SRTransform& t, SRRenderable& r) {
			(void)t;
			const SRModel* model = r.model;

			const u32 numMeshlets = model->numMeshlets;
			gfxDevice.dispatch_mesh(numMeshlets, 1, 1, cmdList);
			// TODO: Transform
			//for (const auto& mesh : model->meshes) {
			//	for (u32 i = mesh.basePrimitive; i < mesh.numPrimitives; ++i) {
			//		const SRMeshPrimitive& primitive = model->primitives[i];s

			//		//gfxDevice.draw_indexed(primitive.numIndices, primitive.baseIndex, primitive.baseVertex, cmdList);
			//	}
			//}
		});
	}

}
