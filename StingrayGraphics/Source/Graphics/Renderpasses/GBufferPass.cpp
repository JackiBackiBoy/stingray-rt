#include "GBufferPass.hpp"
#include "Data/ComponentTypes.hpp"
#include "Data/Model.hpp"

namespace SRGBufferPass {
	struct GBufferPassData {
		SRPipeline pipeline;
		SRShader vertexShader;
		SRShader pixelShader;
	};

	void build(SRRenderPass& self, SRGraphicsDevice& gfxDevice, SRShaderCompiler& shaderCompiler) {
		auto& passData = self.allocate_pass_data<GBufferPassData>();
		shaderCompiler.compile_from_file(RES_DIR "Shaders/GBufferPass.slang", { SRShaderStage::Vertex, "vertexMain" }, passData.vertexShader);
		shaderCompiler.compile_from_file(RES_DIR "Shaders/GBufferPass.slang", { SRShaderStage::Pixel, "pixelMain" }, passData.pixelShader);


		const SRPipelineInfo pipelineInfo = {
			.vertexShader = &passData.vertexShader,
			.pixelShader = &passData.pixelShader,
			.inputLayout = {
				.elements = {
					{ "POSITION", SRFormat::RGB32_FLOAT },
					{ "TEXCOORD", SRFormat::RG32_FLOAT }
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
		gfxDevice.create_pipeline(pipelineInfo, passData.pipeline);
	}

	void execute(SRRenderPass& self, SRGraphicsDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo) {
		auto* passData = self.get_pass_data<GBufferPassData>();

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

			gfxDevice.bind_vertex_buffer(model->vertexBuffer, cmdList);
			gfxDevice.bind_index_buffer(model->indexBuffer, cmdList);

			// TODO: Transform
			for (const auto& mesh : model->meshes) {
				for (uint32_t i = mesh.basePrimitive; i < mesh.numPrimitives; ++i) {
					const SRMeshPrimitive& primitive = model->primitives[i];

					gfxDevice.draw_indexed(primitive.numIndices, primitive.baseIndex, primitive.baseVertex, cmdList);
				}
			}
		});
	}
}
