#include "gfx_device_gl.h"
#include "../../platform.h"

#include <glad/gl.h>

#include <cassert>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>

// GL graphics types
struct Buffer_GL {
	BufferInfo info = {};
	unsigned int id = 0;
};

struct Shader_GL {
	unsigned int id = 0;
	ShaderStage stage = {};
	std::vector<char> data = {};
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

INTERNAL constexpr GLenum to_gl_shaderstage(ShaderStage stage) {
	switch (stage) {
	case ShaderStage::VERTEX:
		return GL_VERTEX_SHADER;
	case ShaderStage::PIXEL:
		return GL_FRAGMENT_SHADER;
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

void GFXDevice_GL::begin_render_pass() {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
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

