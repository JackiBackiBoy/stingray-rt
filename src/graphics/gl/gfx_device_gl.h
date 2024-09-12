#pragma once

#include "../gfx_device.h"

class GFXDevice_GL final : public GFXDevice {
public:
	GFXDevice_GL();
	~GFXDevice_GL();

	void create_swapchain(const SwapChainInfo& info, SwapChain& swapChain) override;
	void create_pipeline(const PipelineInfo& info, Pipeline& pipeline) override;
	void create_buffer(const BufferInfo& info, Buffer& buffer, const void* data) override;
	void create_shader(ShaderStage stage, const std::string& path, Shader& shader) override;
	void create_texture(const TextureInfo& info, Texture& texture, const SubresourceData* data) override;

	void bind_pipeline(const Pipeline& pipeline) override;
	void bind_uniform_buffer(const Buffer& uniformBuffer, uint32_t slot) override;
	void bind_vertex_buffer(const Buffer& vertexBuffer) override;
	void bind_index_buffer(const Buffer& indexBuffer) override;
	void bind_resource(const Resource& resource, uint32_t slot) override;
	void begin_render_pass(const PassInfo& passInfo) override;
	void end_render_pass() override;

	void update_buffer(const Buffer& buffer, const void* data) override;

	void draw(uint32_t vertexCount, uint32_t startVertex) override;
	void draw_indexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex) override;

private:
	struct Impl;
	Impl* m_Impl = nullptr;
};
