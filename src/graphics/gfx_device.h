#pragma once

#include "gfx_types.h"
#include <cstdint>
#include <string>

struct GLFWwindow;

class GFXDevice {
public:
	GFXDevice(GLFWwindow* window) : m_Window(window) {};
	virtual ~GFXDevice() {};

	inline virtual uint32_t get_frame_index() const final { return m_CurrentFrame; }
	inline virtual uint64_t get_frame_count() const final { return m_FrameCount; }

	virtual void create_swapchain(const SwapChainInfo& info, SwapChain& swapChain) = 0;
	virtual void create_pipeline(const PipelineInfo& info, Pipeline& pipeline) = 0;
	virtual void create_buffer(const BufferInfo& info, Buffer& buffer, const void* data) = 0;
	virtual void create_shader(ShaderStage stage, const std::string& path, Shader& shader) = 0;
	virtual void create_texture(const TextureInfo& info, Texture& texture, const SubresourceData* data) = 0;
	virtual void create_sampler(const SamplerInfo& info, Sampler& sampler) = 0;

	// ------------------------------ Ray Tracing ------------------------------
	virtual void create_rtas(const RTASInfo& rtasInfo, RTAS& rtas) = 0;
	virtual void create_rt_instance_buffer(Buffer& buffer, uint32_t numBLASes) = 0;
	virtual void create_rt_pipeline(const RTPipelineInfo& info, RTPipeline& pipeline) = 0;
	virtual void write_blas_instance(const RTTLAS::BLASInstance& instance, void* dst) = 0;

	virtual void bind_pipeline(const Pipeline& pipeline, const CommandList& cmdList) = 0;
	virtual void bind_viewport(const Viewport& viewport, const CommandList& cmdList) = 0;
	virtual void bind_uniform_buffer(const Buffer& uniformBuffer, uint32_t slot) = 0;
	virtual void bind_vertex_buffer(const Buffer& vertexBuffer, const CommandList& cmdList) = 0;
	virtual void bind_index_buffer(const Buffer& indexBuffer, const CommandList& cmdList) = 0;
	virtual void bind_resource(const Resource& resource, uint32_t slot) = 0;
	virtual void push_constants(const void* data, uint32_t size, const CommandList& cmdList) = 0;
	virtual void barrier(const GPUBarrier& barrier, const CommandList& cmdList) = 0;

	virtual CommandList begin_command_list(QueueType queue) = 0;
	virtual void begin_render_pass(const SwapChain& swapChain, const PassInfo& passInfo, const CommandList& cmdList, bool clear = true) = 0;
	virtual void begin_render_pass(const PassInfo& passInfo, const CommandList& cmdList) = 0;
	virtual void end_render_pass(const SwapChain& swapChain, const CommandList& cmdList) = 0;
	virtual void end_render_pass(const CommandList& cmdList) = 0;
	virtual void submit_command_lists(const SwapChain& swapChain) = 0;

	virtual void update_buffer(const Buffer& buffer, const void* data) = 0;

	virtual void draw(uint32_t vertexCount, uint32_t startVertex, const CommandList& cmdList) = 0;
	virtual void draw_indexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex, const CommandList& cmdList) = 0;
	virtual void draw_instanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex, uint32_t startInstance, const CommandList& cmdList) = 0;

	virtual uint32_t get_descriptor_index(const Resource& resource) = 0;
	virtual void wait_for_gpu() = 0;

	static constexpr uint32_t FRAMES_IN_FLIGHT = 3;
	static constexpr uint32_t MAX_UBO_DESCRIPTORS = 32;
	static constexpr uint32_t MAX_TEXTURE_DESCRIPTORS = 1024;
	static constexpr uint32_t MAX_SAMPLER_DESCRIPTORS = 16;
	static constexpr uint32_t MAX_STORAGE_BUFFERS = 32;

protected:
	GLFWwindow* m_Window = nullptr;
	uint32_t m_CurrentImageIndex = 0;
	uint32_t m_CurrentFrame = 0;
	uint64_t m_FrameCount = 0;
};
