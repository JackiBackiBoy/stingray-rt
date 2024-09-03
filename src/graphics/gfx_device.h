#pragma once

#include "gfx_types.h"
#include <cstdint>
#include <string>

class GFXDevice {
public:
    GFXDevice() {};
    virtual ~GFXDevice() {};

    virtual void create_pipeline(const PipelineInfo& info, Pipeline& pipeline) = 0;
    virtual void create_buffer(const BufferInfo& info, Buffer& buffer, const void* data) = 0;
    virtual void create_shader(ShaderStage stage, const std::string& path, Shader& shader) = 0;

    virtual void bind_pipeline(const Pipeline& pipeline) = 0;
    virtual void bind_vertex_buffer(const Buffer& vertexBuffer) = 0;
    virtual void begin_render_pass() = 0;
    virtual void end_render_pass() = 0;

    virtual void draw(uint32_t vertexCount, uint32_t startVertex) = 0;
};
