#pragma once

#include "Core/StringTypes.h"
#include "Core/Types.h"
#include "Core/EnumBitmaskOperators.h"

#define SR_MAX_SWAPCHAIN_IMAGES     3
#define SR_INVALID_DESCRIPTOR_INDEX 0

typedef u32 SRDescriptorIndex;

enum struct SRGFXBackend {
	DX12,
	Vulkan
};

enum SRQueue : u8 {
	SRQueue_Universal, // Graphics + Compute + Copy
	SRQueue_Compute, // Dedicated compute
	SRQueue_Copy, // Dedicated copy queue
	SRQueue_COUNT
};

enum struct SRPipelineStage : u16 {
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
}; SR_ENABLE_BITMASK_OPERATORS(SRPipelineStage);

enum struct SRAccessMask : u16 {
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
}; SR_ENABLE_BITMASK_OPERATORS(SRAccessMask);

enum struct SRBindFlag : u8 {
	None            = 0,
	VertexBuffer    = 1 << 0,
	IndexBuffer     = 1 << 1,
	ConstantBuffer  = 1 << 2,
	ShaderResource  = 1 << 3,
	RenderTarget    = 1 << 4,
	DepthStencil    = 1 << 5,
	UnorderedAccess = 1 << 6,
	ShadingRate     = 1 << 7 // NOTE: Not supported right now
}; SR_ENABLE_BITMASK_OPERATORS(SRBindFlag);

enum struct SRMiscFlag : u8 {
	None              = 0,
	StructuredBuffer  = 1 << 0,
	ByteAddressBuffer = 1 << 1,
	IndirectArgs      = 1 << 2,
	CubeTexture       = 1 << 3,
	RayTracing        = 1 << 4,
}; SR_ENABLE_BITMASK_OPERATORS(SRMiscFlag);

enum struct SRBlend : u8 {
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

enum struct SRBlendOp : u8 {
	Add,
	Subtract,
	RevSubtract,
	Min,
	Max
};

enum struct SRBorderColor : u8 {
	TransparentBlack,
	OpaqueBlack,
	OpaqueWhite
};

enum struct SRComparisonFunc : u8 {
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always
};

enum struct SRCullMode : u8 {
	None,
	Front,
	Back
};

enum struct SRDepthWriteMask : u8 {
	Zero, // Disables depth write
	All // Enables depth write
};

enum struct SRFillMode : u8 {
	Wireframe,
	Solid
};

enum struct SRFilter : u8 {
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

enum struct SRFormat : u8 {
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
	D32_FLOAT_S8X24_UINT,

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
	D32_FLOAT,
	R32_FLOAT,
	R32_UINT,
	R32_SINT,
	D24_UNORM_S8_UINT,
	RGB9E5_SHAREDEXP,

	RG8_UNORM,
	RG8_UINT,
	RG8_SNORM,
	RG8_SINT,
	R16_FLOAT,
	D16_UNORM,
	R16_UNORM,
	R16_UINT,
	R16_SNORM,
	R16_SINT,

	R8_UNORM,
	R8_UINT,
	R8_SNORM,
	R8_SINT,

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

enum struct SRInputClass : u8 {
	PerVertex,
	PerInstance,
};

enum struct SRResourceState : u8 {
	Undefined       = 0,
	ShaderResource  = 1 << 0,
	UnorderedAccess = 1 << 1,
	RenderTarget    = 1 << 2,
	DepthWrite      = 1 << 3,
	DepthRead       = 1 << 4,
	CopySrc         = 1 << 5,
	CopyDst         = 1 << 6,
};

enum struct SRResourceType : u8 {
	Unknown,
	Buffer,
	Texture,
	Sampler
};

enum struct SRLoadOp : u8 {
	None,
	Load,
	Clear,
	DontCare
};

enum struct SRStoreOp : u8 {
	None,
	Store,
	DontCare
};

enum struct SRShaderStage : u8 {
	Vertex,
	Pixel,
	Compute,
	Task, // NOTE: Also referred to as "amplification shader"
	Mesh
};

enum struct SRShaderCompileTarget : u8 {
	SPIRV,
	DXIL,
};

enum struct SRTextureAddressMode : u8 {
	Wrap,
	Mirror,
	Clamp,
	Border,
	MirrorOnce
};

enum struct SRUsage : u8 {
	Default, // CPU no access, GPU read/write
	Upload,  // CPU write, GPU read
	Copy     // Copy from GPU to CPU
};

enum struct SRBarrierType : u8 {
	UAV,
	Image,
	Buffer
};

struct SRSubresourceData {
	const void* data = nullptr;
	u32 rowPitch = 0;
	u32 slicePitch = 0; // NOTE: Only used for 3D textures
};

struct SRSubresourceRange {
	u32 baseMip = 0;
	u32 mipCount = 1;
	u32 baseSlice = 0;
	u32 sliceCount = 1;

	static constexpr SRSubresourceRange All() { return { 0U, ~0U, 0U, ~0U }; }
};

struct SRResource {
	void* internalState = nullptr;
	SRResourceType type = SRResourceType::Unknown;
};

struct SRBufferInfo {
	u64 size = 0;
	u32 stride = 0;
	SRUsage usage = SRUsage::Default;
	SRBindFlag bindFlags = SRBindFlag::None;
	SRMiscFlag miscFlags = SRMiscFlag::None;
};

struct SRBuffer : public SRResource {
	SRBufferInfo info = {};
	void* mappedData = nullptr;
	u64 mappedSize = 0;
};

struct SRTextureInfo {
	u32 width = 1;
	u32 height = 1;
	u32 depth = 1;
	u32 arraySize = 1;
	u32 mipLevels = 1;
	u32 sampleCount = 1;
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
	f32 mipLODBias = 0.0f;
	u32 maxAnisotropy = 0;
	SRComparisonFunc comparisonFunc = SRComparisonFunc::Never;
	SRBorderColor borderColor = SRBorderColor::TransparentBlack;
	f32 minLOD = 0.0f;
	f32 maxLOD;
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

struct SRShader {
	u8* data;
	u64 size;
	Str8 entry_point;
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
		const char* name;
		SRFormat format = SRFormat::Unknown;
		SRInputClass inputClass = SRInputClass::PerVertex;
	};

	Element elements[4];
	u64 num_elements;
};

struct SRRasterizerState {
	SRFillMode fillMode = SRFillMode::Solid;
	SRCullMode cullMode = SRCullMode::None;
	bool frontCW = true;
	bool depthClipEnable = false;
	i32 depthBias = 0;
	f32 depthBiasClamp = 0.0f;
	f32 slopeScaledDepthBias = 0.0f;
	bool multisampleEnable = false;
	bool antialisedLineEnable = false;
};


struct SRPipelineInfo {
	const SRShader* vertexShader = nullptr;
	const SRShader* pixelShader = nullptr;
	const SRShader* computeShader = nullptr;
	const SRShader* meshShader = nullptr;
	const SRShader* taskShader = nullptr;
	SRInputLayout inputLayout = {};
	SRRasterizerState rasterizerState = {};
	SRDepthStencilState depthStencilState = {};
	SRBlendState blendState = {};
	u32 numRenderTargets = 0;
	SRFormat renderTargetFormats[8] = { SRFormat::Unknown };
	SRFormat depthStencilFormat = SRFormat::Unknown;
};

struct SRPipeline {
	SRPipelineInfo info = {};
	void* internalState;
};

struct SRSwapchainInfo {
	u32 width = 0;
	u32 height = 0;
	u32 numBuffers = 3;
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
	void* internalState;
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
	u32 numColorAttachments = 0;
};

struct SRViewport {
	f32 topLeftX = 0.0f;
	f32 topLeftY = 0.0f;
	f32 width = 0.0f;
	f32 height = 0.0f;
	f32 minDepth = 0.0f;
	f32 maxDepth = 1.0f;
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

	inline constexpr u32 get_format_stride(SRFormat format) {
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
