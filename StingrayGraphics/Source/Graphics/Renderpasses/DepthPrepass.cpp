#include "DepthPrepass.h"
#include "Data/ComponentTypes.h"
#include "Data/Model.h"

namespace SRDepthPrepass {
	void build(SRRenderPass& self, SRGFXDevice& gfx_device, SRShaderCompiler& shader_compiler) {
		auto& passData = self.allocate_pass_data<DepthPrepassData>();
		SRShaderCompiler_CompileFromFile(&shader_compiler, Str8_Literal(RES_DIR "Shaders/DepthPrepass.hlsl"), { SRShaderStage::Vertex, Str8_Literal("vertexMain") }, &passData.vertexShader);

		// NOTE: We use Reverse-Z and Infinite Far Plane trick
		const SRPipelineInfo pipelineInfo = {
			.vertexShader = &passData.vertexShader,
			.inputLayout = {
				.elements = {
					{ "POSITION", SRFormat::RGB32_FLOAT },
				},
				.num_elements = 1
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
		SRGFX_CreatePipeline(&gfx_device, &pipelineInfo, &passData.pipeline);
	}

	void execute(SRRenderPass& self, SRGFXDevice& gfx_device, SRCmdList cmd_list, const SRFrameInfo* frame_info) {
		auto* passData = self.get_pass_data<DepthPrepassData>();

		const SRViewport viewport = {
			.width = static_cast<f32>(frame_info->width),
			.height = static_cast<f32>(frame_info->height),
		};

		SRGFX_BindViewport(&gfx_device, &viewport, cmd_list);
		SRGFX_BindPipeline(&gfx_device, &passData->pipeline, cmd_list);
		SRGFX_BindRootConstantBuffer(&gfx_device, frame_info->perFrameBuffer, cmd_list);

		frame_info->scene->for_each<SRTransform, SRRenderable>([&](SRTransform& t, SRRenderable& r) {
			const SRModel* model = r.model;

			SRGFX_BindVertexBuffer(&gfx_device, &model->vertexBuffer, cmd_list);
			SRGFX_BindIndexBuffer(&gfx_device, &model->indexBuffer, cmd_list);

			// TODO: Transform
			for (const auto& mesh : model->meshes) {
				for (u32 i = mesh.basePrimitive; i < mesh.numPrimitives; ++i) {
					const SRMeshPrimitive& primitive = model->primitives[i];

					SRGFX_DrawIndexed(&gfx_device, primitive.numIndices, primitive.baseIndex, primitive.baseVertex, cmd_list);
				}
			}
			});
	}
}
