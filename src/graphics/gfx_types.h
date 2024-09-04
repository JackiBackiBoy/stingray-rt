#pragma once

#include "../enum_flags.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

enum class BindFlag : uint8_t {
	NONE = 0,
	VERTEX_BUFFER = 1 << 0,
	INDEX_BUFFER = 1 << 1,
	UNIFORM_BUFFER = 1 << 2,
	SHADER_RESOURCE = 1 << 3,
	RENDER_TARGET = 1 << 4,
	DEPTH_STENCIL = 1 << 5,
	UNORDERED_ACCESS = 1 << 6,
	SHADING_RATE = 1 << 7,
};

enum class MiscFlag : uint8_t {
	NONE = 0,
	TEXTURECUBE = 1 << 0,
	INDIRECT_ARGS = 1 << 1,
	BUFFER_RAW = 1 << 2,
	BUFFER_STRUCTURED = 1 << 3,
};

enum class Format : uint8_t {
	UNKNOWN,

	R32G32B32A32_FLOAT,
	R32G32B32A32_UINT,
	R32G32B32A32_SINT,

	R32G32B32_FLOAT,
	R32G32B32_UINT,
	R32G32B32_SINT,

	R16G16B16A16_FLOAT,
	R16G16B16A16_UNORM,
	R16G16B16A16_UINT,
	R16G16B16A16_SNORM,
	R16G16B16A16_SINT,

	R32G32_FLOAT,
	R32G32_UINT,
	R32G32_SINT,
	D32_FLOAT_S8X24_UINT,	// depth (32-bit) + stencil (8-bit) | SRV: R32_FLOAT (default or depth aspect), R8_UINT (stencil aspect)

	R10G10B10A2_UNORM,
	R10G10B10A2_UINT,
	R11G11B10_FLOAT,
	R8G8B8A8_UNORM,
	R8G8B8A8_UNORM_SRGB,
	R8G8B8A8_UINT,
	R8G8B8A8_SNORM,
	R8G8B8A8_SINT,
	B8G8R8A8_UNORM,
	B8G8R8A8_UNORM_SRGB,
	R16G16_FLOAT,
	R16G16_UNORM,
	R16G16_UINT,
	R16G16_SNORM,
	R16G16_SINT,
	D32_FLOAT,				// depth (32-bit) | SRV: R32_FLOAT
	R32_FLOAT,
	R32_UINT,
	R32_SINT,
	D24_UNORM_S8_UINT,		// depth (24-bit) + stencil (8-bit) | SRV: R24_INTERNAL (default or depth aspect), R8_UINT (stencil aspect)
	R9G9B9E5_SHAREDEXP,

	R8G8_UNORM,
	R8G8_UINT,
	R8G8_SNORM,
	R8G8_SINT,
	R16_FLOAT,
	D16_UNORM,				// depth (16-bit) | SRV: R16_UNORM
	R16_UNORM,
	R16_UINT,
	R16_SNORM,
	R16_SINT,

	R8_UNORM,
	R8_UINT,
	R8_SNORM,
	R8_SINT,

	// Formats that are not usable in render pass must be below because formats in render pass must be encodable as 6 bits:

	BC1_UNORM,			// Three color channels (5 bits:6 bits:5 bits), with 0 or 1 bit(s) of alpha
	BC1_UNORM_SRGB,		// Three color channels (5 bits:6 bits:5 bits), with 0 or 1 bit(s) of alpha
	BC2_UNORM,			// Three color channels (5 bits:6 bits:5 bits), with 4 bits of alpha
	BC2_UNORM_SRGB,		// Three color channels (5 bits:6 bits:5 bits), with 4 bits of alpha
	BC3_UNORM,			// Three color channels (5 bits:6 bits:5 bits) with 8 bits of alpha
	BC3_UNORM_SRGB,		// Three color channels (5 bits:6 bits:5 bits) with 8 bits of alpha
	BC4_UNORM,			// One color channel (8 bits)
	BC4_SNORM,			// One color channel (8 bits)
	BC5_UNORM,			// Two color channels (8 bits:8 bits)
	BC5_SNORM,			// Two color channels (8 bits:8 bits)
	BC6H_UF16,			// Three color channels (16 bits:16 bits:16 bits) in "half" floating point
	BC6H_SF16,			// Three color channels (16 bits:16 bits:16 bits) in "half" floating point
	BC7_UNORM,			// Three color channels (4 to 7 bits per channel) with 0 to 8 bits of alpha
	BC7_UNORM_SRGB,		// Three color channels (4 to 7 bits per channel) with 0 to 8 bits of alpha

	NV12,				// video YUV420; SRV Luminance aspect: R8_UNORM, SRV Chrominance aspect: R8G8_UNORM
};

enum class InputClass : uint8_t {
	PER_VERTEX,
	PER_INSTANCE,
};

enum class ShaderStage : uint8_t {
	VERTEX,
	PIXEL
};

enum class Usage : uint8_t { // NOTE: Not used for OpenGL
	DEFAULT, // CPU no access, GPU read/write. TIP: Useful for resources that do not change that often or at all
	UPLOAD, // CPU write, GPU read. TIP: Useful for resources that need to be updated frequently (e.g. uniform buffer). Also allows for persistent mapping
	COPY // Copy from GPU to CPU
};

template<>
struct enable_bitmask_operators<BindFlag> { static constexpr bool enable = true; };
template<>
struct enable_bitmask_operators<MiscFlag> { static constexpr bool enable = true; };

struct Resource {
	enum class Type {
		UNKNOWN,
		BUFFER,
		TEXTURE,
		RAYTRACING_AS // NOTE: Not supported in OpenGL
	} type = Type::UNKNOWN;

	void* mappedData = nullptr; // NOTE: Only valid for Usage::UPLOAD
	size_t mappedSize = 0; // NOTE: For buffers: full buffer size; for textures: full texture size including subresources
	std::shared_ptr<void> internalState = nullptr;
};

struct BufferInfo {
	uint64_t size = 0;
	uint32_t stride = 0;
	Usage usage = Usage::DEFAULT; // NOTE: Not used for OpenGL
	BindFlag bindFlags = BindFlag::NONE;
	MiscFlag miscFlags = MiscFlag::NONE;
	bool persistentMap = false; // NOTE: Only considered for Usage::UPLOAD
};

	struct Buffer : public Resource {
		BufferInfo info = {};
	};

struct InputLayout {
	struct Element {
		std::string name = "";
		Format format = Format::UNKNOWN;
		InputClass inputClass = InputClass::PER_VERTEX;
	};

	std::vector<Element> elements = {};
};

struct Shader {
	ShaderStage stage = {};
	std::shared_ptr<void> internalState = nullptr;
};


struct PipelineInfo {
	const Shader* vertexShader = nullptr;
	const Shader* pixelShader = nullptr;
	InputLayout inputLayout = {};
};

struct Pipeline {
	PipelineInfo info = {};
	std::shared_ptr<void> internalState = nullptr;
};

constexpr uint32_t get_format_stride(Format format) {
	switch (format) {
	case Format::BC1_UNORM:
	case Format::BC1_UNORM_SRGB:
	case Format::BC4_SNORM:
	case Format::BC4_UNORM:
		return 8u;

	case Format::R32G32B32A32_FLOAT:
	case Format::R32G32B32A32_UINT:
	case Format::R32G32B32A32_SINT:
	case Format::BC2_UNORM:
	case Format::BC2_UNORM_SRGB:
	case Format::BC3_UNORM:
	case Format::BC3_UNORM_SRGB:
	case Format::BC5_SNORM:
	case Format::BC5_UNORM:
	case Format::BC6H_UF16:
	case Format::BC6H_SF16:
	case Format::BC7_UNORM:
	case Format::BC7_UNORM_SRGB:
		return 16u;

	case Format::R32G32B32_FLOAT:
	case Format::R32G32B32_UINT:
	case Format::R32G32B32_SINT:
		return 12u;

	case Format::R16G16B16A16_FLOAT:
	case Format::R16G16B16A16_UNORM:
	case Format::R16G16B16A16_UINT:
	case Format::R16G16B16A16_SNORM:
	case Format::R16G16B16A16_SINT:
		return 8u;

	case Format::R32G32_FLOAT:
	case Format::R32G32_UINT:
	case Format::R32G32_SINT:
	case Format::D32_FLOAT_S8X24_UINT:
		return 8u;

	case Format::R10G10B10A2_UNORM:
	case Format::R10G10B10A2_UINT:
	case Format::R11G11B10_FLOAT:
	case Format::R8G8B8A8_UNORM:
	case Format::R8G8B8A8_UNORM_SRGB:
	case Format::R8G8B8A8_UINT:
	case Format::R8G8B8A8_SNORM:
	case Format::R8G8B8A8_SINT:
	case Format::B8G8R8A8_UNORM:
	case Format::B8G8R8A8_UNORM_SRGB:
	case Format::R16G16_FLOAT:
	case Format::R16G16_UNORM:
	case Format::R16G16_UINT:
	case Format::R16G16_SNORM:
	case Format::R16G16_SINT:
	case Format::D32_FLOAT:
	case Format::R32_FLOAT:
	case Format::R32_UINT:
	case Format::R32_SINT:
	case Format::D24_UNORM_S8_UINT:
	case Format::R9G9B9E5_SHAREDEXP:
		return 4u;

	case Format::R8G8_UNORM:
	case Format::R8G8_UINT:
	case Format::R8G8_SNORM:
	case Format::R8G8_SINT:
	case Format::R16_FLOAT:
	case Format::D16_UNORM:
	case Format::R16_UNORM:
	case Format::R16_UINT:
	case Format::R16_SNORM:
	case Format::R16_SINT:
		return 2u;

	case Format::R8_UNORM:
	case Format::R8_UINT:
	case Format::R8_SNORM:
	case Format::R8_SINT:
		return 1u;

	default:
		return 16u;
	}
}

