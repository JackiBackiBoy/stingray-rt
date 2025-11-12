#include "TestingPass.hpp"
#include "Graphics/ShaderCompiler.hpp"
#include "Data/Model.hpp"

namespace SRTestingPass {
	struct TestingPassData {
		SRPipeline pipeline;
		SRShader vertexShader;
		SRShader pixelShader;
		SRModel testModel;
	};

	void build(SRRenderPass& self, SRGraphicsDevice& gfxDevice, SRShaderCompiler& shaderCompiler) {
		auto& passData = self.allocate_pass_data<TestingPassData>();
		shaderCompiler.compile_from_file(RES_DIR "Shaders/Testing.slang", { SRShaderStage::VERTEX, "vertexMain" }, passData.vertexShader);
		shaderCompiler.compile_from_file(RES_DIR "Shaders/Testing.slang", { SRShaderStage::PIXEL, "pixelMain" }, passData.pixelShader);
		// TEMPORARY
		SRModelLoader::load_gltf(RES_DIR "Models/Cube/cube.gltf", passData.testModel, gfxDevice);

		const SRPipelineInfo pipelineInfo = {
			.vertexShader = &passData.vertexShader,
			.pixelShader = &passData.pixelShader,
			.inputLayout = {
				.elements = {
					{ "POSITION", SRFormat::RGB32_FLOAT },
					{ "TEXCOORD", SRFormat::RG32_FLOAT }
				}
			},
			.numRenderTargets = 1,
			.renderTargetFormats = { SRFormat::BGRA8_UNORM }
		};
		gfxDevice.create_pipeline(pipelineInfo, passData.pipeline);
	}

	void execute(SRRenderPass& self, SRGraphicsDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo) {
		auto passData = self.get_pass_data<TestingPassData>();

		const SRViewport viewport = {
			.width = static_cast<float>(frameInfo.width),
			.height = static_cast<float>(frameInfo.height),
		};

		gfxDevice.bind_viewport(viewport, cmdList);
		gfxDevice.bind_pipeline(passData->pipeline, cmdList);
		gfxDevice.bind_root_constant_buffer(*frameInfo.perFrameBuffer, cmdList);
		gfxDevice.bind_vertex_buffer(passData->testModel.vertexBuffer, cmdList);
		gfxDevice.bind_index_buffer(passData->testModel.indexBuffer, cmdList);

		for (const auto& mesh : passData->testModel.meshes) {
			for (uint32_t i = mesh.basePrimitive; i < mesh.numPrimitives; ++i) {
				const SRMeshPrimitive& primitive = passData->testModel.primitives[i];

				gfxDevice.draw_indexed(primitive.numIndices, primitive.baseIndex, primitive.baseVertex, cmdList);
			}
		}
	}
}
