#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

typedef uint16_t SRBarrierSync;
typedef uint16_t SRBarrierAccess;
typedef uint8_t SRQueue;
typedef uint8_t SRBindFlag;
typedef uint8_t SRMiscFlag;

typedef uint32_t SRDescriptorIndex;
inline constexpr SRDescriptorIndex INVALID_DESCRIPTOR_INDEX = ~0U;

enum SRBarrierSync_ : SRBarrierSync {
	SRBarrierSync_None          = 0,
	SRBarrierSync_AllCommands   = 1 << 0,
	SRBarrierSync_Draw          = 1 << 1,
	SRBarrierSync_IndexInput    = 1 << 2,
	SRBarrierSync_VertexShader  = 1 << 3,
	SRBarrierSync_PixelShader   = 1 << 4,
	SRBarrierSync_DepthStencil  = 1 << 5,
	SRBarrierSync_RenderTarget  = 1 << 6,
	SRBarrierSync_ComputeShader = 1 << 7,
	SRBarrierSync_RayTracing    = 1 << 8,
	SRBarrierSync_Copy          = 1 << 9,
};

enum SRBarrierAccess_ : SRBarrierAccess {
	SRBarrierAccess_None              = 0,
	SRBarrierAccess_VertexBuffer      = 1 << 0,
	SRBarrierAccess_ConstantBuffer    = 1 << 1,
	SRBarrierAccess_IndexBuffer       = 1 << 2,
	SRBarrierAccess_RenderTarget      = 1 << 3,
	SRBarrierAccess_UnorderedAccess   = 1 << 4,
	SRBarrierAccess_DepthStencilRead  = 1 << 5,
	SRBarrierAccess_DepthStencilWrite = 1 << 6,
	SRBarrierAccess_ShaderResource    = 1 << 7,
	SRBarrierAccess_CopyDest          = 1 << 8,
	SRBarrierAccess_CopySrc           = 1 << 9
};

enum SRQueue_ : SRQueue {
	SRQueue_Universal, // Graphics + Compute + Copy
	SRQueue_Compute, // Dedicated compute
	SRQueue_Copy, // Dedicated copy queue
	SRQueue_COUNT
};

enum SRBindFlag_ : SRBindFlag {
	SRBindFlag_None            = 0,
	SRBindFlag_VertexBuffer    = 1 << 0,
	SRBindFlag_IndexBuffer     = 1 << 1,
	SRBindFlag_ConstantBuffer  = 1 << 2,
	SRBindFlag_ShaderResource  = 1 << 3,
	SRBindFlag_RenderTarget    = 1 << 4,
	SRBindFlag_DepthStencil    = 1 << 5,
	SRBindFlag_UnorderedAccess = 1 << 6,
	SRBindFlag_ShadingRate     = 1 << 7 // NOTE: Not supported right now
};

enum SRMiscFlag_ : SRMiscFlag {
	SRMiscFlag_None              = 0,
	SRMiscFlag_StructuredBuffer  = 1 << 0,
	SRMiscFlag_ByteAddressBuffer = 1 << 1,
	SRMiscFlag_IndirectArgs      = 1 << 2,
	SRMiscFlag_CubeTexture       = 1 << 3,
	SRMiscFlag_RayTracing        = 1 << 4,
};

enum class SRGraphicsAPI : uint8_t {
	DX12,
	VULKAN
};

enum class SRBlend : uint8_t {
	ZERO,
	ONE,
	SRC_COLOR,
	INV_SRC_COLOR,
	SRC_ALPHA,
	INV_SRC_ALPHA,
	DEST_ALPHA,
	INV_DEST_ALPHA,
	DEST_COLOR,
	INV_DEST_COLOR,
	SRC_ALPHA_SAT,
	BLEND_FACTOR,
	INV_BLEND_FACTOR,
	SRC1_COLOR,
	INV_SRC1_COLOR,
	SRC1_ALPHA,
	INV_SRC1_ALPHA
};

enum class SRBlendOp : uint8_t {
	ADD,
	SUBTRACT,
	REV_SUBTRACT,
	MIN,
	MAX
};

enum class SRBorderColor : uint8_t {
	TRANSPARENT_BLACK,
	OPAQUE_BLACK,
	OPAQUE_WHITE
};

enum class SRComparisonFunc : uint8_t {
	NEVER,
	LESS,
	EQUAL,
	LESS_EQUAL,
	GREATER,
	NOT_EQUAL,
	GREATER_EQUAL,
	ALWAYS
};

enum class SRCullMode : uint8_t {
	NONE,
	FRONT,
	BACK
};

enum class SRDepthWriteMask : uint8_t {
	ZERO, // Disables depth write
	ALL // Enables depth write
};

enum class SRFillMode : uint8_t {
	WIREFRAME,
	SOLID
};

enum class SRFilter : uint8_t {
	MIN_MAG_MIP_POINT,
	MIN_MAG_POINT_MIP_LINEAR,
	MIN_POINT_MAG_LINEAR_MIP_POINT,
	MIN_POINT_MAG_MIP_LINEAR,
	MIN_LINEAR_MAG_MIP_POINT,
	MIN_LINEAR_MAG_POINT_MIP_LINEAR,
	MIN_MAG_LINEAR_MIP_POINT,
	MIN_MAG_MIP_LINEAR,
	ANISOTROPIC,
	COMPARISON_MIN_MAG_MIP_POINT,
	COMPARISON_MIN_MAG_POINT_MIP_LINEAR,
	COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT,
	COMPARISON_MIN_POINT_MAG_MIP_LINEAR,
	COMPARISON_MIN_LINEAR_MAG_MIP_POINT,
	COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
	COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
	COMPARISON_MIN_MAG_MIP_LINEAR,
	COMPARISON_ANISOTROPIC,
	MINIMUM_MIN_MAG_MIP_POINT,
	MINIMUM_MIN_MAG_POINT_MIP_LINEAR,
	MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT,
	MINIMUM_MIN_POINT_MAG_MIP_LINEAR,
	MINIMUM_MIN_LINEAR_MAG_MIP_POINT,
	MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
	MINIMUM_MIN_MAG_LINEAR_MIP_POINT,
	MINIMUM_MIN_MAG_MIP_LINEAR,
	MINIMUM_ANISOTROPIC,
	MAXIMUM_MIN_MAG_MIP_POINT,
	MAXIMUM_MIN_MAG_POINT_MIP_LINEAR,
	MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT,
	MAXIMUM_MIN_POINT_MAG_MIP_LINEAR,
	MAXIMUM_MIN_LINEAR_MAG_MIP_POINT,
	MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
	MAXIMUM_MIN_MAG_LINEAR_MIP_POINT,
	MAXIMUM_MIN_MAG_MIP_LINEAR,
	MAXIMUM_ANISOTROPIC
};

enum class SRFormat : uint8_t {
	UNKNOWN,

	RGBA32_FLOAT,
	RGBA32_UINT,
	RGBA32_SINT,

	RGB32_FLOAT,
	RGB32_UINT,
	RGB32_SINT,

	RGBA16_FLOAT,
	RGBA16_UNORM,
	RGBA16_UINT,
	RGBA16_SNORM,
	RGBA16_SINT,

	RG32_FLOAT,
	RG32_UINT,
	RG32_SINT,
	D32_FLOAT_S8X24_UINT, // depth (32-bit) + stencil (8-bit) | SRV: R32_FLOAT (default or depth aspect), R8_UINT (stencil aspect)

	RGB10A2_UNORM,
	RGB10A2_UINT,
	RG11B10_FLOAT,
	RGBA8_UNORM,
	RGBA8_UNORM_SRGB,
	RGBA8_UINT,
	RGBA8_SNORM,
	RGBA8_SINT,
	BGRA8_UNORM,
	BGRA8_UNORM_SRGB,
	RG16_FLOAT,
	RG16_UNORM,
	RG16_UINT,
	RG16_SNORM,
	RG16_SINT,
	D32_FLOAT,			// depth (32-bit) | SRV: R32_FLOAT
	R32_FLOAT,
	R32_UINT,
	R32_SINT,
	D24_UNORM_S8_UINT,	// depth (24-bit) + stencil (8-bit) | SRV: R24_INTERNAL (default or depth aspect), R8_UINT (stencil aspect)
	RGB9E5_SHAREDEXP,

	RG8_UNORM,
	RG8_UINT,
	RG8_SNORM,
	RG8_SINT,
	R16_FLOAT,
	D16_UNORM,			// depth (16-bit) | SRV: R16_UNORM
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

	NV12				// video YUV420; SRV Luminance aspect: R8_UNORM, SRV Chrominance aspect: R8G8_UNORM
};

enum class SRInputClass : uint8_t {
	PER_VERTEX,
	PER_INSTANCE,
};

enum class SRResourceState : uint8_t {
	UNDEFINED        = 0,
	SHADER_RESOURCE  = 1 << 0,
	UNORDERED_ACCESS = 1 << 1,
	RENDER_TARGET    = 1 << 2,
	DEPTH_WRITE      = 1 << 3,
	DEPTH_READ       = 1 << 4,
	COPY_SRC         = 1 << 5,
	COPY_DST         = 1 << 6,
};

enum class SRResourceType : uint8_t {
	Unknown,
	Buffer,
	Texture,
	Sampler
};

enum class SRLoadOp : uint8_t {
	None,
	Load,
	Clear,
	DontCare
};

enum class SRStoreOp : uint8_t {
	None,
	Store,
	DontCare
};

enum class SRShaderStage : uint8_t {
	VERTEX,
	PIXEL,
	COMPUTE
};

enum class SRShaderCompileTarget : uint8_t {
	UNKNOWN,
	GLSL,
	HLSL,
	SPIRV,
	DXIL
};

enum class SRTextureAddressMode : uint8_t {
	WRAP,
	MIRROR,
	CLAMP,
	BORDER,
	MIRROR_ONCE
};

enum class SRUsage : uint8_t {
	DEFAULT, // CPU no access, GPU read/write
	UPLOAD, // CPU write, GPU read
	COPY // Copy from GPU to CPU
};

enum class SRBarrierType : uint8_t {
	UAV,
	IMAGE,
	BUFFER
};

struct SRSubresourceData {
	const void* data = nullptr;
	uint32_t rowPitch = 0;
	uint32_t slicePitch = 0; // NOTE: Only used for 3D textures
};

struct SRSubresourceRange {
	uint32_t baseMip = 0;
	uint32_t mipCount = 1;
	uint32_t baseSlice = 0;
	uint32_t sliceCount = 1;

	static constexpr SRSubresourceRange All() {
		return { 0U, ~0U, 0U, ~0U };
	}
};

struct SRResource {
	std::shared_ptr<void> internalState = nullptr;
	SRResourceType type = SRResourceType::Unknown;
};

struct SRBufferInfo {
	uint64_t size = 0;
	uint32_t stride = 0;
	SRUsage usage = SRUsage::DEFAULT;
	SRBindFlag bindFlags = SRBindFlag_None;
	SRMiscFlag miscFlags = SRMiscFlag_None;
};

struct SRBuffer : public SRResource {
	SRBufferInfo info = {};
	void* mappedData = nullptr;
	uint64_t mappedSize = 0;
};

struct SRTextureInfo {
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t depth = 1;
	uint32_t arraySize = 1;
	uint32_t mipLevels = 1;
	uint32_t sampleCount = 1;
	SRFormat format = SRFormat::UNKNOWN;
	SRUsage usage = SRUsage::DEFAULT;
	SRBindFlag bindFlags = SRBindFlag_None;
};

struct SRTexture : public SRResource {
	SRTextureInfo info = {};
};

struct SRSamplerInfo {
	SRFilter filter = SRFilter::MIN_MAG_MIP_LINEAR;
	SRTextureAddressMode addressU = SRTextureAddressMode::WRAP;
	SRTextureAddressMode addressV = SRTextureAddressMode::WRAP;
	SRTextureAddressMode addressW = SRTextureAddressMode::WRAP;
	float mipLODBias = 0.0f;
	uint32_t maxAnisotropy = 0;
	SRComparisonFunc comparisonFunc = SRComparisonFunc::NEVER;
	SRBorderColor borderColor = SRBorderColor::TRANSPARENT_BLACK;
	float minLOD = 0.0f;
	float maxLOD = std::numeric_limits<float>::max();
};

struct SRSampler : public SRResource {
	SRSamplerInfo info = {};
};

struct SRBarrier {
	SRBarrierType type = SRBarrierType::IMAGE;

	struct UAV {
		// TODO
	};

	struct Image {
		const SRTexture* texture = nullptr;
		SRResourceState stateBefore = SRResourceState::UNDEFINED;
		SRResourceState stateAfter = SRResourceState::UNDEFINED;
		SRBarrierAccess accessBefore = SRBarrierAccess_None;
		SRBarrierAccess accessAfter = SRBarrierAccess_None;
		SRBarrierSync syncBefore = SRBarrierSync_None;
		SRBarrierSync syncAfter = SRBarrierSync_None;
		SRSubresourceRange subresourceRange = {};
	};

	struct Buffer {
		// TODO
	};

	union {
		UAV uav;
		Image image;
		Buffer buffer;
	};
};

struct SRShaderPlatformInfo {
	SRShaderCompileTarget target = SRShaderCompileTarget::UNKNOWN;
	const char* profileName = nullptr;
	// TODO: Add features such a block scalar layout and such
};

struct SRShader {
	std::vector<uint8_t> byteCode;
};

struct SRBlendState {
	bool alphaToCoverage = false;
	bool independentBlend = false;

	struct RenderTargetBlendState {
		bool blendEnable = false;
		SRBlend srcBlend = SRBlend::SRC_ALPHA;
		SRBlend dstBlend = SRBlend::INV_SRC_ALPHA;
		SRBlendOp blendOp = SRBlendOp::ADD;
		SRBlend srcBlendAlpha = SRBlend::ONE;
		SRBlend dstBlendAlpha = SRBlend::ONE;
		SRBlendOp blendOpAlpha = SRBlendOp::ADD;
	};
	RenderTargetBlendState renderTargetBlendStates[8];
};

struct SRDepthStencilState {
	bool depthEnable = false;
	bool stencilEnable = false;
	SRDepthWriteMask depthWriteMask = SRDepthWriteMask::ZERO;
	SRComparisonFunc depthFunction = SRComparisonFunc::NEVER;
};

struct SRInputLayout {
	struct Element {
		std::string name;
		SRFormat format = SRFormat::UNKNOWN;
		SRInputClass inputClass = SRInputClass::PER_VERTEX;
	};

	std::vector<Element> elements = {};
};

struct SRRasterizerState {
	SRFillMode fillMode = SRFillMode::SOLID;
	SRCullMode cullMode = SRCullMode::NONE;
	bool frontCW = true;
	bool depthClipEnable = false;
	int32_t depthBias = 0;
	float depthBiasClamp = 0.0f;
	float slopeScaledDepthBias = 0.0f;
	bool multisampleEnable = false;
	bool antialisedLineEnable = false;
};


struct SRPipelineInfo {
	const SRShader* vertexShader = nullptr;
	const SRShader* pixelShader = nullptr;
	const SRShader* computeShader = nullptr;
	SRInputLayout inputLayout = {};
	SRRasterizerState rasterizerState = {};
	SRDepthStencilState depthStencilState = {};
	SRBlendState blendState = {};
	uint32_t numRenderTargets = 0;
	SRFormat renderTargetFormats[8] = { SRFormat::UNKNOWN };
	SRFormat depthStencilFormat = SRFormat::UNKNOWN;
};

struct SRPipeline {
	SRPipelineInfo info = {};
	std::shared_ptr<void> internalState;
};

struct SRSwapchainInfo {
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t numBuffers = 3;
	SRFormat format = SRFormat::RGBA8_UNORM;
	bool vSync = true;
	bool fullscreen = false;
	bool useHDR = false;
};

struct SRCmdList {
	void* internalState = nullptr;
};

struct SRSwapchain {
	SRSwapchainInfo info = {};
	std::shared_ptr<void> internalState;
};

struct SRPassInfo {
	struct Attachment {
		const SRTexture* texture = nullptr;
		float clearValue = 0.0f;
		SRLoadOp loadOp = SRLoadOp::Clear;
		SRStoreOp storeOp = SRStoreOp::Store;
	};

	Attachment colorAttachments[8] = {};
	Attachment depthAttachment = {};
	uint32_t numColorAttachments = 0;
};

struct SRViewport {
	float topLeftX = 0.0f;
	float topLeftY = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	float minDepth = 0.0f;
	float maxDepth = 1.0f;
};

namespace SRGraphicsHelpers {
	inline constexpr bool is_depth_format(SRFormat format) {
		switch (format) {
		case SRFormat::D16_UNORM:
		case SRFormat::D24_UNORM_S8_UINT:
		case SRFormat::D32_FLOAT:
		case SRFormat::D32_FLOAT_S8X24_UINT:
			return true;
		default:
			return false;
		}
	}

	inline constexpr uint32_t get_format_stride(SRFormat format) {
		switch (format) {
		case SRFormat::BC1_UNORM:
		case SRFormat::BC1_UNORM_SRGB:
		case SRFormat::BC4_SNORM:
		case SRFormat::BC4_UNORM:
			return 8;

		case SRFormat::RGBA32_FLOAT:
		case SRFormat::RGBA32_UINT:
		case SRFormat::RGBA32_SINT:
		case SRFormat::BC2_UNORM:
		case SRFormat::BC2_UNORM_SRGB:
		case SRFormat::BC3_UNORM:
		case SRFormat::BC3_UNORM_SRGB:
		case SRFormat::BC5_SNORM:
		case SRFormat::BC5_UNORM:
		case SRFormat::BC6H_UF16:
		case SRFormat::BC6H_SF16:
		case SRFormat::BC7_UNORM:
		case SRFormat::BC7_UNORM_SRGB:
			return 16;

		case SRFormat::RGB32_FLOAT:
		case SRFormat::RGB32_UINT:
		case SRFormat::RGB32_SINT:
			return 12;

		case SRFormat::RGBA16_FLOAT:
		case SRFormat::RGBA16_UNORM:
		case SRFormat::RGBA16_UINT:
		case SRFormat::RGBA16_SNORM:
		case SRFormat::RGBA16_SINT:
			return 8;

		case SRFormat::RG32_FLOAT:
		case SRFormat::RG32_UINT:
		case SRFormat::RG32_SINT:
		case SRFormat::D32_FLOAT_S8X24_UINT:
			return 8;

		case SRFormat::RGB10A2_UNORM:
		case SRFormat::RGB10A2_UINT:
		case SRFormat::RG11B10_FLOAT:
		case SRFormat::RGBA8_UNORM:
		case SRFormat::RGBA8_UNORM_SRGB:
		case SRFormat::RGBA8_UINT:
		case SRFormat::RGBA8_SNORM:
		case SRFormat::RGBA8_SINT:
		case SRFormat::BGRA8_UNORM:
		case SRFormat::BGRA8_UNORM_SRGB:
		case SRFormat::RG16_FLOAT:
		case SRFormat::RG16_UNORM:
		case SRFormat::RG16_UINT:
		case SRFormat::RG16_SNORM:
		case SRFormat::RG16_SINT:
		case SRFormat::D32_FLOAT:
		case SRFormat::R32_FLOAT:
		case SRFormat::R32_UINT:
		case SRFormat::R32_SINT:
		case SRFormat::D24_UNORM_S8_UINT:
		case SRFormat::RGB9E5_SHAREDEXP:
			return 4;

		case SRFormat::RG8_UNORM:
		case SRFormat::RG8_UINT:
		case SRFormat::RG8_SNORM:
		case SRFormat::RG8_SINT:
		case SRFormat::R16_FLOAT:
		case SRFormat::D16_UNORM:
		case SRFormat::R16_UNORM:
		case SRFormat::R16_UINT:
		case SRFormat::R16_SNORM:
		case SRFormat::R16_SINT:
			return 2;

		case SRFormat::R8_UNORM:
		case SRFormat::R8_UINT:
		case SRFormat::R8_SNORM:
		case SRFormat::R8_SINT:
			return 1;

		default:
			return 16;
		}
	}
}