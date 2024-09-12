#include "gfx_device_gl.h"
#include "../../platform.h"

#include <glad/gl.h>

#include <cassert>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>

// GL graphics types
struct Resource_GL {
	unsigned int id = 0;
};

struct Buffer_GL : public Resource_GL {
	BufferInfo info = {};
};

struct Shader_GL {
	unsigned int id = 0;
	ShaderStage stage = {};
	std::vector<char> data = {};
};

struct Texture_GL : public Resource_GL {
};

struct Pipeline_GL {
	PipelineInfo info = {};
	unsigned int linkedShaderID = 0;
	unsigned int vaoID = 0;
};

// GL converter functions
INTERNAL Buffer_GL* to_internal(const Buffer& buffer) {
	return (Buffer_GL*)buffer.internalState.get();
}

INTERNAL Shader_GL* to_internal(const Shader& shader) {
	return (Shader_GL*)shader.internalState.get();
}

INTERNAL Pipeline_GL* to_internal(const Pipeline& pipeline) {
	return (Pipeline_GL*)pipeline.internalState.get();
}

INTERNAL Resource_GL* to_internal(const Resource& resource) {
	return (Resource_GL*)resource.internalState.get();
}

INTERNAL constexpr GLenum to_gl_shaderstage(ShaderStage stage) {
	switch (stage) {
	case ShaderStage::VERTEX:
		return GL_VERTEX_SHADER;
	case ShaderStage::PIXEL:
		return GL_FRAGMENT_SHADER;
	}

	return 0;
}

INTERNAL constexpr GLint to_gl_internal_format(Format value) {
	switch (value) {
	case Format::UNKNOWN:
		return 0;
	case Format::R32G32B32A32_FLOAT:
		return GL_RGBA32F;
	case Format::R32G32B32A32_UINT:
		return GL_RGBA32UI;
	case Format::R32G32B32A32_SINT:
		return GL_RGBA32I;
	case Format::R32G32B32_FLOAT:
		return GL_RGB32F;
	case Format::R32G32B32_UINT:
		return GL_RGB32UI;
	case Format::R32G32B32_SINT:
		return GL_RGB32I;
	case Format::R16G16B16A16_FLOAT:
		return GL_RGBA16F;
	case Format::R16G16B16A16_UNORM:
		return GL_RGBA16;
	case Format::R16G16B16A16_UINT:
		return GL_RGBA16UI;
	case Format::R16G16B16A16_SNORM:
		return GL_RGBA16_SNORM;
	case Format::R16G16B16A16_SINT:
		return GL_RGBA16I;
	case Format::R32G32_FLOAT:
		return GL_RG32F;
	case Format::R32G32_UINT:
		return GL_RG32UI;
	case Format::R32G32_SINT:
		return GL_RG32I;
	case Format::D32_FLOAT_S8X24_UINT:
		return 0;
	case Format::R10G10B10A2_UNORM:
		return 0;
	case Format::R10G10B10A2_UINT:
		return 0;
	case Format::R11G11B10_FLOAT:
		return 0;
	case Format::R8G8B8A8_UNORM:
		return GL_RGBA8;
	case Format::R8G8B8A8_UNORM_SRGB:
		return 0;
	case Format::R8G8B8A8_UINT:
		return GL_RGBA8I;
	case Format::R8G8B8A8_SNORM:
		return GL_RGBA8_SNORM;
	case Format::R8G8B8A8_SINT:
		return GL_RGBA8I;
	case Format::R16G16_FLOAT:
		return GL_RG16F;
	case Format::R16G16_UNORM:
		return GL_RG16;
	case Format::R16G16_UINT:
		return GL_RG16UI;
	case Format::R16G16_SNORM:
		return GL_RG16_SNORM;
	case Format::R16G16_SINT:
		return GL_RG16I;
	case Format::D32_FLOAT:
		return GL_DEPTH_COMPONENT32F;
	case Format::R32_FLOAT:
		return GL_R32F;
	case Format::R32_UINT:
		return GL_R32UI;
	case Format::R32_SINT:
		return GL_R32I;
	case Format::D24_UNORM_S8_UINT:
		return GL_DEPTH24_STENCIL8;
	case Format::R9G9B9E5_SHAREDEXP:
		return 0;
	case Format::R8G8_UNORM:
		return GL_RG8;
	case Format::R8G8_UINT:
		return GL_RG8UI;
	case Format::R8G8_SNORM:
		return GL_RG8_SNORM;
	case Format::R8G8_SINT:
		return GL_RG8I;
	case Format::R16_FLOAT:
		return GL_R16F;
	case Format::D16_UNORM:
		return GL_DEPTH_COMPONENT16;
	case Format::R16_UNORM:
		return GL_R16;
	case Format::R16_UINT:
		return GL_R16UI;
	case Format::R16_SNORM:
		return GL_R16_SNORM;
	case Format::R16_SINT:
		return GL_R16I;
	case Format::R8_UNORM:
		return GL_R8;
	case Format::R8_UINT:
		return GL_R8UI;
	case Format::R8_SNORM:
		return GL_R8_SNORM;
	case Format::R8_SINT:
		return GL_R8I;
	case Format::BC1_UNORM:
		return GL_R8;
	case Format::BC1_UNORM_SRGB:
		return 0;
	case Format::BC2_UNORM:
		return 0;
	case Format::BC2_UNORM_SRGB:
		return 0;
	case Format::BC3_UNORM:
		return 0;
	case Format::BC3_UNORM_SRGB:
		return 0;
	case Format::BC4_UNORM:
		return 0;
	case Format::BC4_SNORM:
		return 0;
	case Format::BC5_UNORM:
		return 0;
	case Format::BC5_SNORM:
		return 0;
	case Format::B8G8R8A8_UNORM:
		return GL_BGRA; // TODO: Might be wrong
	case Format::B8G8R8A8_UNORM_SRGB:
		return 0;
	case Format::BC6H_UF16:
		return 0;
	case Format::BC6H_SF16:
		return 0;
	case Format::BC7_UNORM:
		return 0;
	case Format::BC7_UNORM_SRGB:
		return 0;
	case Format::NV12:
		return 0;
	}
	return 0;
}

INTERNAL constexpr GLint to_gl_format(Format value) {
	switch (value) {
	case Format::UNKNOWN:
		return 0;
	case Format::R32G32B32A32_FLOAT:
	case Format::R32G32B32A32_UINT:
	case Format::R32G32B32A32_SINT:
	case Format::R16G16B16A16_FLOAT:
	case Format::R16G16B16A16_UNORM:
	case Format::R16G16B16A16_UINT:
	case Format::R16G16B16A16_SNORM:
	case Format::R16G16B16A16_SINT:
	case Format::R8G8B8A8_UNORM:
	case Format::R8G8B8A8_UNORM_SRGB:
	case Format::R8G8B8A8_UINT:
	case Format::R8G8B8A8_SNORM:
	case Format::R8G8B8A8_SINT:
		return GL_RGBA;
	case Format::R32G32B32_FLOAT:
	case Format::R32G32B32_UINT:
	case Format::R32G32B32_SINT:
		return GL_RGB;
	case Format::R32G32_FLOAT:
	case Format::R32G32_UINT:
	case Format::R32G32_SINT:
	case Format::R16G16_FLOAT:
	case Format::R16G16_UNORM:
	case Format::R16G16_UINT:
	case Format::R16G16_SNORM:
	case Format::R16G16_SINT:
	case Format::R8G8_UNORM:
	case Format::R8G8_UINT:
	case Format::R8G8_SNORM:
	case Format::R8G8_SINT:
		return GL_RG;
	case Format::R32_FLOAT:
	case Format::R32_UINT:
	case Format::R32_SINT:
	case Format::R16_FLOAT:
	case Format::R16_UNORM:
	case Format::R16_UINT:
	case Format::R16_SNORM:
	case Format::R16_SINT:
	case Format::R8_UNORM:
	case Format::R8_UINT:
	case Format::R8_SNORM:
	case Format::R8_SINT:
		return GL_RED;
	case Format::D32_FLOAT:
	case Format::D24_UNORM_S8_UINT:
	case Format::D16_UNORM:
		return GL_DEPTH;
	}
	return 0;
}

// Pointer to implementation
struct GFXDevice_GL::Impl {
	void initialize() {};

	Pipeline_GL* currentPipeline = nullptr;
	uint32_t numUBOBindings = 0;
};

// GFXDevice_GL Interface
GFXDevice_GL::GFXDevice_GL() {
	m_Impl = new Impl();
	m_Impl->initialize();
}

GFXDevice_GL::~GFXDevice_GL() {
	delete m_Impl;
}

void GFXDevice_GL::create_swapchain(const SwapChainInfo& info, SwapChain& swapChain) {
	swapChain.info = info;
}

void GFXDevice_GL::create_pipeline(const PipelineInfo& info, Pipeline& pipeline) {
	auto internalState = std::make_shared<Pipeline_GL>();
	internalState->info = info;

	pipeline.info = info;
	pipeline.internalState = internalState;

	// Create VAO
	glGenVertexArrays(1, &internalState->vaoID);
	glBindVertexArray(internalState->vaoID);

	// Shaders
	internalState->linkedShaderID = glCreateProgram();
	if (info.vertexShader != nullptr) {
		auto internalShader = to_internal(*info.vertexShader);
		glAttachShader(internalState->linkedShaderID, internalShader->id);
	}
	if (info.pixelShader != nullptr) {
		auto internalShader = to_internal(*info.pixelShader);
		glAttachShader(internalState->linkedShaderID, internalShader->id);
	}

	glLinkProgram(internalState->linkedShaderID);

	// Cleanup shaders after linking
	if (info.vertexShader != nullptr) {
		auto internalShader = to_internal(*info.vertexShader);
		glDeleteShader(internalShader->id);
	}
	if (info.pixelShader != nullptr) {
		auto internalShader = to_internal(*info.pixelShader);
		glDeleteShader(internalShader->id);
	}

	int success = 0;
	char infoLog[512];
	glGetProgramiv(internalState->linkedShaderID, GL_LINK_STATUS, &success);
	if(!success) {
		glGetProgramInfoLog(internalState->linkedShaderID, 512, NULL, infoLog);
	}

	glBindVertexArray(0);
}

void GFXDevice_GL::create_buffer(const BufferInfo& info, Buffer& buffer, const void* data) {
	auto internalState = std::make_shared<Buffer_GL>();
	internalState->info = info;

	buffer.info = info;
	buffer.mappedSize = 0;
	buffer.mappedData = nullptr;
	buffer.type = Resource::Type::BUFFER;
	buffer.internalState = internalState;

	GLenum bindingTarget = 0;

	if (has_flag(buffer.info.bindFlags, BindFlag::VERTEX_BUFFER)) {
		bindingTarget = GL_ARRAY_BUFFER;
	}
	else if (has_flag(buffer.info.bindFlags, BindFlag::INDEX_BUFFER)) {
		bindingTarget = GL_ELEMENT_ARRAY_BUFFER;
	}
	else if (has_flag(buffer.info.bindFlags, BindFlag::UNIFORM_BUFFER)) {
		bindingTarget = GL_UNIFORM_BUFFER;
		++m_Impl->numUBOBindings;
	}

	// TODO: Allow for other options than GL_STATIC_DRAW
	glGenBuffers(1, &internalState->id);
	glBindBuffer(bindingTarget, internalState->id);
	glBufferData(bindingTarget, info.size, data, GL_STATIC_DRAW);
}

void GFXDevice_GL::create_shader(ShaderStage stage, const std::string& path, Shader& shader) {
	auto internalState = std::make_shared<Shader_GL>();
	internalState->stage = stage;

	shader.stage = stage;
	shader.internalState = internalState;

	std::ifstream file(path, std::ios::ate);
	if (!file.is_open()) {
		throw std::runtime_error("SHADER ERROR: Failed to open shader file");
	}

	const size_t fileSize = static_cast<size_t>(file.tellg());
	internalState->data.resize(fileSize);

	file.seekg(0);
	file.read(internalState->data.data(), fileSize);
	file.close();
	internalState->data.push_back('\0');

	internalState->id = glCreateShader(to_gl_shaderstage(stage));
	const GLchar* shaderSource = internalState->data.data();
	glShaderSource(internalState->id, 1, &shaderSource, nullptr);
	glCompileShader(internalState->id);

	int  success;
	char infoLog[512];
	glGetShaderiv(internalState->id, GL_COMPILE_STATUS, &success);

	if (!success) {
		glGetShaderInfoLog(internalState->id, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}
}

void GFXDevice_GL::create_texture(const TextureInfo& info, Texture& texture, const SubresourceData* data) {
	auto internalState = std::make_shared<Texture_GL>();

	texture.info = info;
	texture.mappedSize = 0;
	texture.mappedData = nullptr;
	texture.type = Resource::Type::TEXTURE;
	texture.internalState = internalState;

	glGenTextures(1, &internalState->id);
	glBindTexture(GL_TEXTURE_2D, internalState->id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if (data != nullptr) {
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			to_gl_internal_format(info.format),
			info.width,
			info.height,
			0,
			to_gl_format(info.format),
			GL_UNSIGNED_BYTE,
			data->data
		);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
}

void GFXDevice_GL::bind_pipeline(const Pipeline& pipeline) {
	auto internalPipeline = to_internal(pipeline);

	glUseProgram(internalPipeline->linkedShaderID);
	glBindVertexArray(internalPipeline->vaoID);
	m_Impl->currentPipeline = internalPipeline;
}

void GFXDevice_GL::bind_uniform_buffer(const Buffer& uniformBuffer, uint32_t slot) {
	assert(m_Impl->currentPipeline != nullptr);
	const PipelineInfo& pipelineInfo = m_Impl->currentPipeline->info;

	auto internalUniformBuffer = to_internal(uniformBuffer);
	glBindBufferRange(GL_UNIFORM_BUFFER, slot, internalUniformBuffer->id, 0, uniformBuffer.info.size);

	//int maxBindings = 0;
	//glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &maxBindings);

	GLuint blockIndex = glGetUniformBlockIndex(m_Impl->currentPipeline->linkedShaderID, "PerFrameData");
	assert(blockIndex != GL_INVALID_INDEX);

	glUniformBlockBinding(m_Impl->currentPipeline->linkedShaderID, blockIndex, slot);
}

void GFXDevice_GL::bind_vertex_buffer(const Buffer& vertexBuffer) {
	assert(m_Impl->currentPipeline != nullptr);
	const PipelineInfo& pipelineInfo = m_Impl->currentPipeline->info;

	auto internalVertexBuffer = to_internal(vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, internalVertexBuffer->id);

	// TRICK: Due to the fact that OpenGL does not have a PSO like
	// Vulkan and DX12, we manually re-set the input layout from
	// the currently bound "pipeline"
	uintptr_t offset = 0; // TODO: Make non-float types supported
	for (size_t i = 0; i < pipelineInfo.inputLayout.elements.size(); ++i) {
		const InputLayout::Element& element = pipelineInfo.inputLayout.elements[i];
		const int formatStride = get_format_stride(element.format);
		const int numFloats = formatStride / sizeof(float);

		glVertexAttribPointer(i, numFloats, GL_FLOAT, GL_FALSE, vertexBuffer.info.stride, (void*)offset);
		glEnableVertexAttribArray(i);
		offset += formatStride;
	}
}

void GFXDevice_GL::bind_index_buffer(const Buffer& indexBuffer) {
	assert(m_Impl->currentPipeline != nullptr);
	const PipelineInfo& pipelineInfo = m_Impl->currentPipeline->info;

	auto internalIndexBuffer = to_internal(indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, internalIndexBuffer->id);
}

void GFXDevice_GL::bind_resource(const Resource& resource, uint32_t slot) {
	auto internalResource = to_internal(resource);

	if (resource.type == Resource::Type::TEXTURE) {
		auto internalTexture = (Texture_GL*)internalResource;

		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, internalResource->id);
	}
}

void GFXDevice_GL::begin_render_pass(const PassInfo& passInfo) {
	GLbitfield bitfield = GL_COLOR_BUFFER_BIT; // TODO: Perhaps not clear buffer in all cases

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GFXDevice_GL::end_render_pass() {
}

void GFXDevice_GL::update_buffer(const Buffer& buffer, const void* data) {
	assert(buffer.info.usage == Usage::UPLOAD); // TODO: not really a criteria

	auto internalBuffer = to_internal(buffer);
	GLenum bindingTarget = 0;

	if (has_flag(buffer.info.bindFlags, BindFlag::UNIFORM_BUFFER)) {
		bindingTarget = GL_UNIFORM_BUFFER;
	}

	glBindBuffer(bindingTarget, internalBuffer->id);
	// TODO: Allow for offset and dynamic size in bytes
	glBufferSubData(bindingTarget, 0, buffer.info.size, data);
}

void GFXDevice_GL::draw(uint32_t vertexCount, uint32_t startVertex) {
	glDrawArrays(GL_TRIANGLES, startVertex, vertexCount);
}

void GFXDevice_GL::draw_indexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex) {
	// NOTE: uint32_t assumed for index-type
	const uintptr_t byteOffset = (startIndex * sizeof(uint32_t));
	glDrawElementsBaseVertex(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, (void*)byteOffset, baseVertex);
}

