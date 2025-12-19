#include "DepthPrepass.h"
#include "Data/ComponentTypes.h"
#include "Data/Model.h"

namespace SRDepthPrepass {
	struct DepthPrepassData {
		SRPipeline pipeline;
		SRShader vertexShader;
	};

	void build(SRRenderPass& self, SRGraphicsDevice& gfxDevice, SRShaderCompiler& shaderCompiler) {
		auto& passData = self.allocate_pass_data<DepthPrepassData>();
		shaderCompiler.compile_from_file(RES_DIR "Shaders/DepthPrepass.slang", { SRShaderStage::Vertex, "vertexMain" }, passData.vertexShader);

		// NOTE: We use Reverse-Z and Infinite Far Plane trick
		const SRPipelineInfo pipelineInfo = {
			.vertexShader = &passData.vertexShader,
			.inputLayout = {
				.elements = {
					{ "POSITION", SRFormat::RGB32_FLOAT },
					{ "TEXCOORD", SRFormat::RG32_FLOAT }, // TODO: Stride it instead
				}
			},
			.rasterizerState = {
				.cullMode = SRCullMode::Back,
				.depthClipEnable = true
			},
			.depthStencilState = {
				.depthEnable = true,
				.stencilEnable = false,
				.depthWriteMask = SRDepthWriteMask::All,
				.depthFunction = SRComparisonFunc::Greater
			},
			.depthStencilFormat = SRFormat::D32_FLOAT
		};
		gfxDevice.create_pipeline(pipelineInfo, passData.pipeline);
	}

	void execute(SRRenderPass& self, SRGraphicsDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo) {
		auto* passData = self.get_pass_data<DepthPrepassData>();

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
				for (u32 i = mesh.basePrimitive; i < mesh.numPrimitives; ++i) {
					const SRMeshPrimitive& primitive = model->primitives[i];

					gfxDevice.draw_indexed(primitive.numIndices, primitive.baseIndex, primitive.baseVertex, cmdList);
				}
			}
			});
	}
}
