#pragma once

#include "Graphics/GraphicsDevice.h"

class SRGraphicsDevice_DX12 final : public SRGraphicsDevice {
public:
	SRGraphicsDevice_DX12(SRWindow& window);
	~SRGraphicsDevice_DX12();

	u32 get_frame_index() const override;

	void create_swapchain(const SRSwapchainInfo& info, SRSwapchain& swapchain) override;
	void create_pipeline(const SRPipelineInfo& info, SRPipeline& pipeline) override;
	void create_buffer(const SRBufferInfo& info, SRBuffer& buffer, const void* data) override;
	void create_texture(const SRTextureInfo& info, SRTexture& texture, const SRSubresourceData* data) override;
	void create_sampler(const SRSamplerInfo& info, SRSampler& sampler) override;

	void bind_pipeline(const SRPipeline& pipeline, const SRCmdList& cmdList) override;
	void bind_viewport(const SRViewport& viewport, const SRCmdList& cmdList) override;
	void bind_vertex_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) override;
	void bind_index_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) override;
	void bind_root_constant_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) override;
	void push_constants(const void* data, u32 size, const SRCmdList& cmdList) override;
	void barrier(const SRBarrier* pBarriers, u32 numBarriers, const SRCmdList& cmdList) override;

	void begin_frame(const SRSwapchain& swapchain) override;
	SRCmdList begin_command_list(SRQueue queue) override;
	void begin_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) override;
	void begin_render_pass(const SRPassInfo& passInfo, const SRCmdList& cmdList) override;
	void end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) override;
	void end_render_pass(const SRCmdList& cmdList) override;
	void submit_command_lists(const SRSwapchain& swapchain) override;

	void draw(u32 vtxCount, u32 startVtx, const SRCmdList& cmdList) override;
	void draw_indexed(u32 idxCount, u32 startIdx, u32 baseVtx, const SRCmdList& cmdList) override;
	void dispatch_mesh(u32 groupCountX, u32 groupCountY, u32 groupCountZ, const SRCmdList& cmdList) override;

	SRDescriptorIndex get_descriptor_index_srv(const SRResource& resource) override;
	SRShaderPlatformInfo get_shader_platform_info() override;
	void wait_for_gpu() override;
	void flush_initial_uploads() override;
	void setup_imgui_init_info(SRFormat swapchainFormat) override;

private:
	struct Impl;
	Impl* m_Impl;
};