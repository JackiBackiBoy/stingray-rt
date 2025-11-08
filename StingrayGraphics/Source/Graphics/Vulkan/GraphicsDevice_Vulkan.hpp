#pragma once

#include "Graphics/GraphicsDevice.hpp"

class SRGraphicsDevice_Vulkan final : public SRGraphicsDevice {
public:
	SRGraphicsDevice_Vulkan(SRWindow& window);
	~SRGraphicsDevice_Vulkan();

	uint32_t get_frame_index() const override;

	void create_swapchain(const SRSwapchainInfo& info, SRSwapchain& swapchain) override;
	void create_pipeline(const SRPipelineInfo& info, SRPipeline& pipeline) override;
	void create_buffer(const SRBufferInfo& info, SRBuffer& buffer, const void* data) override;

	void bind_pipeline(const SRPipeline& pipeline, const SRCmdList& cmdList) override;
	void bind_viewport(const SRViewport& viewport, const SRCmdList& cmdList) override;
	void bind_vertex_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) override;
	void bind_index_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) override;
	void bind_root_constant_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) override;

	SRCmdList begin_command_list(SRQueue queue) override;
	void begin_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) override;
	void end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) override;
	void submit_command_lists(const SRSwapchain& swapchain) override;

	void draw(uint32_t vtxCount, uint32_t startVtx, const SRCmdList& cmdList) override;
	void draw_indexed(uint32_t idxCount, uint32_t startIdx, uint32_t baseVtx, const SRCmdList& cmdList) override;

	SRShaderPlatformInfo get_shader_platform_info() override;
	void wait_for_gpu() override;
	void flush_initial_uploads() override;

private:
	struct Impl;
	Impl* m_Impl;
};