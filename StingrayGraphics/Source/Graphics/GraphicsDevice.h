#pragma once

#include "Core/Types.h"
#include "Core/Window.h"
#include "Graphics/GraphicsTypes.h"

class SRGraphicsDevice {
public:
	SRGraphicsDevice(SRWindow& window) : m_Window(window) {};
	virtual ~SRGraphicsDevice() = default;

	virtual u32 get_frame_index() const = 0;

	virtual void create_swapchain(const SRSwapchainInfo& info, SRSwapchain& swapchain) = 0;
	virtual void create_pipeline(const SRPipelineInfo& info, SRPipeline& pipeline) = 0;
	virtual void create_buffer(const SRBufferInfo& info, SRBuffer& buffer, const void* data) = 0;
	virtual void create_texture(const SRTextureInfo& info, SRTexture& texture, const SRSubresourceData* data) = 0;
	virtual void create_sampler(const SRSamplerInfo& info, SRSampler& sampler) = 0;

	virtual void bind_pipeline(const SRPipeline& pipeline, const SRCmdList& cmdList) = 0;
	virtual void bind_viewport(const SRViewport& viewport, const SRCmdList& cmdList) = 0;
	virtual void bind_vertex_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) = 0;
	virtual void bind_index_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) = 0;
	virtual void bind_root_constant_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) = 0;
	virtual void push_constants(const void* data, u32 size, const SRCmdList& cmdList) = 0;
	virtual void barrier(const SRBarrier* pBarriers, u32 numBarriers, const SRCmdList& cmdList) = 0;

	virtual SRCmdList begin_command_list(SRQueue quee) = 0;
	virtual void begin_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) = 0;
	virtual void begin_render_pass(const SRPassInfo& passInfo, const SRCmdList& cmdList) = 0;
	virtual void end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) = 0;
	virtual void end_render_pass(const SRCmdList& cmdList) = 0;
	virtual void submit_command_lists(const SRSwapchain& swapchain) = 0;

	virtual void draw(u32 vtxCount, u32 startVtx, const SRCmdList& cmdList) = 0;
	virtual void draw_indexed(u32 idxCount, u32 startIdx, u32 baseVtx, const SRCmdList& cmdList) = 0;
	virtual void dispatch_mesh(u32 groupCountX, u32 groupCountY, u32 groupCountZ, const SRCmdList& cmdList) = 0;

	virtual SRDescriptorIndex get_descriptor_index_srv(const SRResource& resource) = 0;
	virtual SRShaderPlatformInfo get_shader_platform_info() = 0;
	virtual void wait_for_gpu() = 0;
	virtual void flush_initial_uploads() = 0; // NOTE: TEMPORARY function, will be removed once we introduce streaming system
	virtual void setup_imgui_init_info(SRFormat swapchainFormat) = 0;

	static constexpr u32 FRAMES_IN_FLIGHT = 2;
	
protected:
	SRWindow& m_Window;
};
