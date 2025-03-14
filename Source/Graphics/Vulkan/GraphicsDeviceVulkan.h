#pragma once

#include "Graphics/GraphicsDevice.h"

namespace SR {
	class GraphicsDeviceVulkan final : public GraphicsDevice {
	public:
		GraphicsDeviceVulkan(Window& window);
		~GraphicsDeviceVulkan();

		GraphicsDeviceVulkan(const GraphicsDeviceVulkan&) = delete;
		GraphicsDeviceVulkan& operator=(const GraphicsDeviceVulkan&) = delete;
		GraphicsDeviceVulkan(GraphicsDeviceVulkan&&) = delete;
		GraphicsDeviceVulkan& operator=(GraphicsDeviceVulkan&&) = delete;

		// TODO: Destroy shader modules when not needed
		// --------------------------- Resource Creation ---------------------------
		void create_swapchain(const SwapChainInfo& info, SwapChain& swapChain) override;
		void create_pipeline(const PipelineInfo& info, Pipeline& pipeline) override;
		void create_buffer(const BufferInfo& info, Buffer& buffer, const void* data) override;
		void create_shader(ShaderStage stage, const std::string& path, Shader& shader) override;
		void create_texture(const TextureInfo& info, Texture& texture, const SubresourceData* data) override;
		void create_sampler(const SamplerInfo& info, Sampler& sampler) override;

		// ------------------------------ Ray Tracing ------------------------------
		void create_rtas(const RTASInfo& rtasInfo, RTAS& rtas) override;
		void create_rt_instance_buffer(Buffer& buffer, uint32_t numBLASes) override;
		void create_rt_pipeline(const RTPipelineInfo& info, RTPipeline& pipeline) override;
		void create_shader_binding_table(const RTPipeline& pipeline, uint32_t groupID, ShaderBindingTable& sbt) override;
		void write_blas_instance(const RTTLAS::BLASInstance& instance, void* dst) override;
		void build_rtas(RTAS& rtas, const CommandList& cmdList) override;
		void bind_rt_pipeline(const RTPipeline& pipeline, const CommandList& cmdList) override;
		void push_rt_constants(const void* data, uint32_t size, const RTPipeline& pipeline, const CommandList& cmdList) override;
		void dispatch_rays(const DispatchRaysInfo& info, const CommandList& cmdList) override;

		// ------------------- Pipeline State & Resource Binding -------------------
		void bind_pipeline(const Pipeline& pipeline, const CommandList& cmdList) override;
		void bind_viewport(const Viewport& viewport, const CommandList& cmdList) override;
		void bind_uniform_buffer(const Buffer& uniformBuffer, uint32_t slot) override;
		void bind_vertex_buffer(const Buffer& vertexBuffer, const CommandList& cmdList) override;
		void bind_index_buffer(const Buffer& indexBuffer, const CommandList& cmdList) override;
		void push_constants(const void* data, uint32_t size, const CommandList& cmdList) override;
		void barrier(const GPUBarrier& barrier, const CommandList& cmdList) override;

		// ------------------------ Commands & Renderpasses ------------------------
		CommandList begin_command_list(QueueType queue) override;
		void begin_render_pass(const SwapChain& swapChain, const PassInfo& passInfo, const CommandList& cmdList, bool clear) override;
		void begin_render_pass(const PassInfo& passInfo, const CommandList& cmdList) override;
		void end_render_pass(const SwapChain& swapChain, const CommandList& cmdList) override;
		void end_render_pass(const CommandList& cmdList) override;
		void submit_command_lists(const SwapChain& swapChain) override;

		// ----------------------------- Draw Commands -----------------------------
		void draw(uint32_t vertexCount, uint32_t startVertex, const CommandList& cmdList) override;
		void draw_indexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex, const CommandList& cmdList) override;
		void draw_instanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex, uint32_t startInstance, const CommandList& cmdList) override;

		// ----------------------------- Miscellaneous -----------------------------
		uint32_t get_descriptor_index(const Resource& resource, SubresourceType type) override;
		uint64_t get_bda(const Buffer& buffer) override;
		void wait_for_gpu() override;

	private:
		struct Impl;
		Impl* m_Impl = nullptr;
	};
}
