#pragma once

#include "Core/EnumBitmaskOperators.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

typedef uint32_t SRDescriptorIndex;
inline constexpr SRDescriptorIndex INVALID_DESCRIPTOR_INDEX = ~0U;

enum SRQueue : uint8_t {
	SRQueue_Universal, // Graphics + Compute + Copy
	SRQueue_Compute, // Dedicated compute
	SRQueue_Copy, // Dedicated copy queue
	SRQueue_COUNT
};

enum class SRPipelineStage : uint16_t {
	None          = 0,
	AllCommands   = 1 << 0,
	Draw          = 1 << 1,
	IndexInput    = 1 << 2,
	VertexShader  = 1 << 3,
	PixelShader   = 1 << 4,
	DepthStencil  = 1 << 5,
	RenderTarget  = 1 << 6,
	ComputeShader = 1 << 7,
	RayTracing    = 1 << 8,
	Copy          = 1 << 9,
};

enum class SRAccessMask : uint16_t {
	None              = 0,
	VertexBuffer      = 1 << 0,
	ConstantBuffer    = 1 << 1,
	IndexBuffer       = 1 << 2,
	RenderTarget      = 1 << 3,
	UnorderedAccess   = 1 << 4,
	DepthStencilRead  = 1 << 5,
	DepthStencilWrite = 1 << 6,
	ShaderResource    = 1 << 7,
	CopyDst           = 1 << 8,
	CopySrc           = 1 << 9
};

enum class SRBindFlag : uint8_t {
	None            = 0,
	VertexBuffer    = 1 << 0,
	IndexBuffer     = 1 << 1,
	ConstantBuffer  = 1 << 2,
	ShaderResource  = 1 << 3,
	RenderTarget    = 1 << 4,
	DepthStencil    = 1 << 5,
	UnorderedAccess = 1 << 6,
	ShadingRate     = 1 << 7 // NOTE: Not supported right now
};

enum class SRMiscFlag : uint8_t {
	None              = 0,
	StructuredBuffer  = 1 << 0,
	ByteAddressBuffer = 1 << 1,
	IndirectArgs      = 1 << 2,
	CubeTexture       = 1 << 3,
	RayTracing        = 1 << 4,
};

SR_ENABLE_BITMASK_OPERATORS(SRPipelineStage);
SR_ENABLE_BITMASK_OPERATORS(SRAccessMask);
SR_ENABLE_BITMASK_OPERATORS(SRBindFlag);
SR_ENABLE_BITMASK_OPERATORS(SRMiscFlag);

enum class SRGraphicsAPI : uint8_t {
	DX12,
	Vulkan
};

enum class SRBlend : uint8_t {
	Zero,
	One,
	SrcColor,
	InvSrcColor,
	SrcAlpha,
	InvSrcAlpha,
	DstAlpha,
	InvDstAlpha,
	DstColor,
	InvDstColor,
	SrcAlphaSat,
	BlendFactor,
	InvBlendFator,
	Src1Color,
	InvSrc1Color,
	Src1Alpha,
	InvSrc1Alpha
};

enum class SRBlendOp : uint8_t {
	Add,
	Subtract,
	RevSubtract,
	Min,
	Max
};

enum class SRBorderColor : uint8_t {
	TransparentBlack,
	OpaqueBlack,
	OpaqueWhite
};

enum class SRComparisonFunc : uint8_t {
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always
};

enum class SRCullMode : uint8_t {
	None,
	Front,
	Back
};

enum class SRDepthWriteMask : uint8_t {
	Zero, // Disables depth write
	All // Enables depth write
};

enum class SRFillMode : uint8_t {
	Wireframe,
	Solid
};

enum class SRFilter : uint8_t {
	MinMagMipPoint,
	MinMagPointMipLinear,
	MinPointMagLinearMipPoint,
	MinPointMagMipLinear,
	MinLinearMagMipPoint,
	MinLinearMagPointMipLinear,
	MinMagLinearMipPoint,
	MinMagMipLinear,
	Anisotropic,
	ComparisonMinMagMipPoint,
	ComparisonMinMagPointMipLinear,
	ComparisonMinPointMagLinearMipPoint,
	ComparisonMinPointMagMipLinear,
	ComparisonMinLinearMagMipPoint,
	ComparisonMinLinearMagPointMipLinear,
	ComparisonMinMagLinearMipPoint,
	ComparisonMinMagMipLinear,
	ComparisonAnisotropic,
	MinimumMinMagMipPoint,
	MinimumMinMagPointMipLinear,
	MinimumMinPointMagLinearMipPoint,
	MinimumMinPointMagMipLinear,
	MinimumMinLinearMagMipPoint,
	MinimumMinLinearMagPointMipLinear,
	MinimumMinMagLinearMipPoint,
	MinimumMinMagMipLinear,
	MinimumAnisotropic,
	MaximumMinMagMipPoint,
	MaximumMinMagPointMipLinear,
	MaximumMinPointMagLinearMipPoint,
	MaximumMinPointMagMipLinear,
	MaximumMinLinearMagMipPoint,
	MaximumMinLinearMagPointMipLinear,
	MaximumMinMagLinearMipPoint,
	MaximumMinMagMipLinear,
	MaximumAnisotropic
};

enum class SRFormat : uint8_t {
	Unknown,

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
	PerVertex,
	PerInstance,
};

enum class SRResourceState : uint8_t {
	Undefined       = 0,
	ShaderResource  = 1 << 0,
	UnorderedAccess = 1 << 1,
	RenderTarget    = 1 << 2,
	DepthWrite      = 1 << 3,
	DepthRead       = 1 << 4,
	CopySrc         = 1 << 5,
	CopyDst         = 1 << 6,
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
	Vertex,
	Pixel,
	Compute
};

enum class SRShaderCompileTarget : uint8_t {
	Unknown,
	GLSL,
	HLSL,
	SPIRV,
	DXIL
};

enum class SRTextureAddressMode : uint8_t {
	Wrap,
	Mirror,
	Clamp,
	Border,
	MirrorOnce
};

enum class SRUsage : uint8_t {
	Default, // CPU no access, GPU read/write
	Upload, // CPU write, GPU read
	Copy // Copy from GPU to CPU
};

enum class SRBarrierType : uint8_t {
	UAV,
	Image,
	Buffer
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

	static constexpr SRSubresourceRange All() { return { 0U, ~0U, 0U, ~0U }; }
};

struct SRResource {
	std::shared_ptr<void> internalState = nullptr;
	SRResourceType type = SRResourceType::Unknown;
};

struct SRBufferInfo {
	uint64_t size = 0;
	uint32_t stride = 0;
	SRUsage usage = SRUsage::Default;
	SRBindFlag bindFlags = SRBindFlag::None;
	SRMiscFlag miscFlags = SRMiscFlag::None;
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
	SRFormat format = SRFormat::Unknown;
	SRUsage usage = SRUsage::Default;
	SRBindFlag bindFlags = SRBindFlag::None;
};

struct SRTexture : public SRResource {
	SRTextureInfo info = {};
};

struct SRSamplerInfo {
	SRFilter filter = SRFilter::MinMagMipLinear;
	SRTextureAddressMode addressU = SRTextureAddressMode::Wrap;
	SRTextureAddressMode addressV = SRTextureAddressMode::Wrap;
	SRTextureAddressMode addressW = SRTextureAddressMode::Wrap;
	float mipLODBias = 0.0f;
	uint32_t maxAnisotropy = 0;
	SRComparisonFunc comparisonFunc = SRComparisonFunc::Never;
	SRBorderColor borderColor = SRBorderColor::TransparentBlack;
	float minLOD = 0.0f;
	float maxLOD = std::numeric_limits<float>::max();
};

struct SRSampler : public SRResource {
	SRSamplerInfo info = {};
};

struct SRBarrier {
	SRBarrierType type = SRBarrierType::Image;

	struct UAV {
		// TODO
	};

	struct Image {
		const SRTexture* texture = nullptr;
		SRResourceState stateBefore = SRResourceState::Undefined;
		SRResourceState stateAfter = SRResourceState::Undefined;
		SRAccessMask accessBefore = SRAccessMask::None;
		SRAccessMask accessAfter = SRAccessMask::None;
		SRPipelineStage syncBefore = SRPipelineStage::None;
		SRPipelineStage syncAfter = SRPipelineStage::None;
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
	SRShaderCompileTarget target = SRShaderCompileTarget::Unknown;
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
		SRBlend srcBlend = SRBlend::SrcAlpha;
		SRBlend dstBlend = SRBlend::InvSrcAlpha;
		SRBlendOp blendOp = SRBlendOp::Add;
		SRBlend srcBlendAlpha = SRBlend::One;
		SRBlend dstBlendAlpha = SRBlend::One;
		SRBlendOp blendOpAlpha = SRBlendOp::Add;
	};
	RenderTargetBlendState renderTargetBlendStates[8];
};

struct SRDepthStencilState {
	bool depthEnable = false;
	bool stencilEnable = false;
	SRDepthWriteMask depthWriteMask = SRDepthWriteMask::Zero;
	SRComparisonFunc depthFunction = SRComparisonFunc::Never;
};

struct SRInputLayout {
	struct Element {
		std::string name;
		SRFormat format = SRFormat::Unknown;
		SRInputClass inputClass = SRInputClass::PerVertex;
	};

	std::vector<Element> elements = {};
};

struct SRRasterizerState {
	SRFillMode fillMode = SRFillMode::Solid;
	SRCullMode cullMode = SRCullMode::None;
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
	SRFormat renderTargetFormats[8] = { SRFormat::Unknown };
	SRFormat depthStencilFormat = SRFormat::Unknown;
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
