#pragma once

#include "Core/Window.hpp"
#include "Graphics/GraphicsTypes.hpp"
#include <cstdint>

class SRGraphicsDevice {
public:
	SRGraphicsDevice(SRWindow& window) : m_Window(window) {};
	virtual ~SRGraphicsDevice() = default;

	virtual uint32_t get_frame_index() const = 0;

	virtual void create_swapchain(const SRSwapchainInfo& info, SRSwapchain& swapchain) = 0;
	virtual void create_pipeline(const SRPipelineInfo& info, SRPipeline& pipeline) = 0;
	virtual void create_buffer(const SRBufferInfo& info, SRBuffer& buffer, const void* data) = 0;

	virtual void bind_pipeline(const SRPipeline& pipeline, const SRCmdList& cmdList) = 0;
	virtual void bind_viewport(const SRViewport& viewport, const SRCmdList& cmdList) = 0;
	virtual void bind_vertex_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) = 0;
	virtual void bind_index_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) = 0;
	virtual void bind_root_constant_buffer(const SRBuffer& buffer, const SRCmdList& cmdList) = 0;

	virtual SRCmdList begin_command_list(SRQueue quee) = 0;
	virtual void begin_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) = 0;
	virtual void end_render_pass(const SRSwapchain& swapchain, const SRCmdList& cmdList) = 0;
	virtual void submit_command_lists(const SRSwapchain& swapchain) = 0;

	virtual void draw(uint32_t vtxCount, uint32_t startVtx, const SRCmdList& cmdList) = 0;
	virtual void draw_indexed(uint32_t idxCount, uint32_t startIdx, uint32_t baseVtx, const SRCmdList& cmdList) = 0;

	virtual SRShaderPlatformInfo get_shader_platform_info() = 0;
	virtual void wait_for_gpu() = 0;
	virtual void flush_initial_uploads() = 0; // NOTE: TEMPORARY function, will be removed once we introduce streaming system
	virtual void setup_imgui_init_info(SRFormat swapchainFormat) = 0;

	static constexpr uint32_t FRAMES_IN_FLIGHT = 2;
	
protected:
	SRWindow& m_Window;
};
