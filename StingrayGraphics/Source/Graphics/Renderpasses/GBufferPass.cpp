#include "GBufferPass.h"
#include "Data/ComponentTypes.h"
#include "Data/Model.h"

namespace SRGBufferPass {
	struct GBufferPassData {
		SRPipeline pipeline;
		SRShader vertexShader;
		SRShader pixelShader;
	};

	void build(SRRenderPass& self, SRGFXDevice& gfxDevice, SRShaderCompiler& shaderCompiler) {
		auto& passData = self.allocate_pass_data<GBufferPassData>();
		shaderCompiler.compile_from_file(RES_DIR "Shaders/GBufferPass.hlsl", { SRShaderStage::Vertex, "vertexMain" }, passData.vertexShader);
		shaderCompiler.compile_from_file(RES_DIR "Shaders/GBufferPass.hlsl", { SRShaderStage::Pixel, "pixelMain" }, passData.pixelShader);


		const SRPipelineInfo pipelineInfo = {
			.vertexShader = &passData.vertexShader,
			.pixelShader = &passData.pixelShader,
			.inputLayout = {
				.elements = {
					{ "POSITION", SRFormat::RGB32_FLOAT }
				}
			},
			.rasterizerState = {
				.cullMode = SRCullMode::Back,
				.depthClipEnable = true
			},
			.depthStencilState = {
				.depthEnable = true,
				.stencilEnable = false,
				.depthWriteMask = SRDepthWriteMask::Zero,
				.depthFunction = SRComparisonFunc::Equal
			},
			.numRenderTargets = 1,
			.renderTargetFormats = { SRFormat::RGBA8_UNORM },
			.depthStencilFormat = SRFormat::D32_FLOAT
		};
		SRGFX_CreatePipeline(&gfxDevice, &pipelineInfo, &passData.pipeline);
	}

	void execute(SRRenderPass& self, SRGFXDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo) {
		auto* passData = self.get_pass_data<GBufferPassData>();

		const SRViewport viewport = {
			.width = static_cast<float>(frameInfo.width),
			.height = static_cast<float>(frameInfo.height),
		};

		SRGFX_BindViewport(&gfxDevice, &viewport, &cmdList);
		SRGFX_BindPipeline(&gfxDevice, &passData->pipeline, &cmdList);
		SRGFX_BindRootConstantBuffer(&gfxDevice, frameInfo.perFrameBuffer, &cmdList);

		frameInfo.scene->for_each<SRTransform, SRRenderable>([&](SRTransform& t, SRRenderable& r) {
			(void)t;
			const SRModel* model = r.model;

			SRGFX_BindVertexBuffer(&gfxDevice, &model->vertexBuffer, &cmdList);
			SRGFX_BindIndexBuffer(&gfxDevice, &model->indexBuffer, &cmdList);

			// TODO: Transform
			for (const auto& mesh : model->meshes) {
				for (u32 i = mesh.basePrimitive; i < mesh.numPrimitives; ++i) {
					const SRMeshPrimitive& primitive = model->primitives[i];

					SRGFX_DrawIndexed(&gfxDevice, primitive.numIndices, primitive.baseIndex, primitive.baseVertex, &cmdList);
				}
			}
		});
	}
}
