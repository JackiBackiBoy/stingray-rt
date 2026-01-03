#pragma once

#include "Data/ArenaAllocator.h"
#include "Core/Logger.h"
#include "Core/Types.h"
#include "Graphics/GraphicsTypes.h"

#include "d3d12.h"
#include <D3D12MemAlloc.h>
#include <dxgi1_6.h>
#include <Windows.h>

#include <assert.h>

#define HR(hr) do { HRESULT _hr = (hr); assert(SUCCEEDED(_hr)); } while (0)

struct SRDescriptorHeap_DX12 {
	u32 count;
	u32 capacity;
	D3D12_DESCRIPTOR_HEAP_TYPE heapType;
	u32 descriptorHandleSize;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandleStart;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandleStart;
	ID3D12DescriptorHeap* heapObject;
};

SRDescriptorHeap_DX12*      SRDescriptorHeap_DX12_Create(SRArena* arena, ID3D12Device* d3d12Device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 capacity);
SRDescriptorIndex           SRDescriptorHeap_DX12_GetNextIndex(SRDescriptorHeap_DX12* heap);
D3D12_CPU_DESCRIPTOR_HANDLE SRDescriptorHeap_DX12_GetCPUHandle(SRDescriptorHeap_DX12* heap, SRDescriptorIndex index);
D3D12_GPU_DESCRIPTOR_HANDLE SRDescriptorHeap_DX12_GetGPUHandle(SRDescriptorHeap_DX12* heap, SRDescriptorIndex index);
SRDescriptorIndex           SRDescriptorHeap_DX12_GetIndexFromCPUHandle(SRDescriptorHeap_DX12* heap, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle);
SRDescriptorIndex           SRDescriptorHeap_DX12_GetIndexFromGPUHandle(SRDescriptorHeap_DX12* heap, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);
void                        SRDescriptorHeap_DX12_Destroy(SRDescriptorHeap_DX12* heap);

struct SRDestructionHandler_DX12 {
	SRArray* objects;
};

SRDestructionHandler_DX12* SRDestructionHandler_DX12_Create(SRArena* arena);
void SRDestructionhandler_DX12_Update(SRDestructionHandler_DX12* handler, u64 frameCount, u64 frameExpiration);
void SRDestructionHandler_DX12_Enqueue(SRDestructionHandler_DX12* handler, IUnknown* obj);
void SRDestructionHandler_DX12_Destroy(SRDestructionHandler_DX12* handler);

struct SRResource_DX12 {
	D3D12MA::Allocation* allocation = nullptr;
};

struct SRBuffer_DX12 : public SRResource_DX12 {
	SRDescriptorIndex srvDescriptor = INVALID_DESCRIPTOR_INDEX;
};

struct SRTexture_DX12 : public SRResource_DX12 {
	SRDescriptorIndex rtvDescriptor = INVALID_DESCRIPTOR_INDEX;
	SRDescriptorIndex srvDescriptor = INVALID_DESCRIPTOR_INDEX;
	SRDescriptorIndex dsvDescriptor = INVALID_DESCRIPTOR_INDEX;
	SRDescriptorIndex dsvReadOnlyDescriptor = INVALID_DESCRIPTOR_INDEX;
};

struct SRSampler_DX12 {
	SRDescriptorIndex samplerDescriptor = INVALID_DESCRIPTOR_INDEX;
};

struct SRCmdList_DX12 {
	ID3D12GraphicsCommandList7* graphicsCmdList;
};

struct SRPipeline_DX12 {
	ID3D12PipelineState* pipeline;
	ID3D12RootSignature* rootSignature;
};

struct SRSwapchain_DX12 {
	IDXGISwapChain3* swapchain;
	ID3D12Resource* images[SR_MAX_SWAPCHAIN_IMAGES];
	SRDescriptorIndex rtvDescriptors[SR_MAX_SWAPCHAIN_IMAGES];
	u32 imageCount;
};

// ---------------------------- Converter Functions ----------------------------
inline SRBuffer_DX12* to_dx12_internal(const SRBuffer& buffer) {
	return (SRBuffer_DX12*)buffer.internalState;
}

inline SRTexture_DX12* to_dx12_internal(const SRTexture& texture) {
	return (SRTexture_DX12*)texture.internalState;
}

inline SRCmdList_DX12* to_dx12_internal(const SRCmdList& cmdList) {
	return (SRCmdList_DX12*)cmdList.internalState;
}

inline SRPipeline_DX12* to_dx12_internal(const SRPipeline& pipeline) {
	return (SRPipeline_DX12*)pipeline.internalState;
}

inline SRSwapchain_DX12* to_dx12_internal(const SRSwapchain& swapchain) {
	return (SRSwapchain_DX12*)swapchain.internalState;
}

inline constexpr D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE to_dx12_load_op(SRLoadOp value) {
	switch (value) {
	case SRLoadOp::None:
		return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
	case SRLoadOp::Load:
		return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
	case SRLoadOp::Clear:
		return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
	case SRLoadOp::DontCare:
		return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
	default:
		return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
	}
}

inline constexpr D3D12_RENDER_PASS_ENDING_ACCESS_TYPE to_dx12_store_op(SRStoreOp value) {
	switch (value) {
	case SRStoreOp::None:
		return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
	case SRStoreOp::Store:
		return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
	case SRStoreOp::DontCare:
		return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
	default:
		return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
	}
}

inline constexpr D3D12_BLEND to_dx12_blend(SRBlend value) {
	switch (value) {
	case SRBlend::Zero:
		return D3D12_BLEND_ZERO;
	case SRBlend::One:
		return D3D12_BLEND_ONE;
	case SRBlend::SrcColor:
		return D3D12_BLEND_SRC_COLOR;
	case SRBlend::InvSrcColor:
		return D3D12_BLEND_INV_SRC_COLOR;
	case SRBlend::SrcAlpha:
		return D3D12_BLEND_SRC_ALPHA;
	case SRBlend::InvSrcAlpha:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case SRBlend::DstAlpha:
		return D3D12_BLEND_DEST_ALPHA;
	case SRBlend::InvDstAlpha:
		return D3D12_BLEND_INV_DEST_ALPHA;
	case SRBlend::DstColor:
		return D3D12_BLEND_DEST_COLOR;
	case SRBlend::InvDstColor:
		return D3D12_BLEND_INV_DEST_COLOR;
	case SRBlend::SrcAlphaSat:
		return D3D12_BLEND_SRC_ALPHA_SAT;
	case SRBlend::BlendFactor:
		return D3D12_BLEND_BLEND_FACTOR;
	case SRBlend::InvBlendFator:
		return D3D12_BLEND_INV_BLEND_FACTOR;
	case SRBlend::Src1Color:
		return D3D12_BLEND_SRC1_COLOR;
	case SRBlend::InvSrc1Color:
		return D3D12_BLEND_INV_SRC1_COLOR;
	case SRBlend::Src1Alpha:
		return D3D12_BLEND_SRC1_ALPHA;
	case SRBlend::InvSrc1Alpha:
		return D3D12_BLEND_INV_SRC1_ALPHA;
	default:
		return D3D12_BLEND_ZERO;
	}
}

inline constexpr D3D12_BLEND to_dx12_alpha_blend(SRBlend value) {
	switch (value) {
	case SRBlend::SrcColor:
		return D3D12_BLEND_SRC_ALPHA;
	case SRBlend::InvSrcColor:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case SRBlend::DstColor:
		return D3D12_BLEND_DEST_ALPHA;
	case SRBlend::InvDstColor:
		return D3D12_BLEND_INV_DEST_ALPHA;
	case SRBlend::Src1Color:
		return D3D12_BLEND_SRC1_ALPHA;
	case SRBlend::InvSrc1Color:
		return D3D12_BLEND_INV_SRC1_ALPHA;
	default:
		return to_dx12_blend(value);
	}
}

inline constexpr D3D12_BLEND_OP to_dx12_blend_op(SRBlendOp value) {
	switch (value) {
	case SRBlendOp::Add:
		return D3D12_BLEND_OP_ADD;
	case SRBlendOp::Subtract:
		return D3D12_BLEND_OP_SUBTRACT;
	case SRBlendOp::RevSubtract:
		return D3D12_BLEND_OP_REV_SUBTRACT;
	case SRBlendOp::Min:
		return D3D12_BLEND_OP_MIN;
	case SRBlendOp::Max:
		return D3D12_BLEND_OP_MAX;
	default:
		return D3D12_BLEND_OP_ADD;
	}
}

inline constexpr D3D12_COMMAND_LIST_TYPE to_dx12_cmd_list_type(SRQueue queue) {
	switch (queue) {
	case SRQueue_Universal:
		return D3D12_COMMAND_LIST_TYPE_DIRECT;
	case SRQueue_Compute:
		return D3D12_COMMAND_LIST_TYPE_COMPUTE;
	case SRQueue_Copy:
		return D3D12_COMMAND_LIST_TYPE_COPY;
	default:
		return D3D12_COMMAND_LIST_TYPE_NONE;
	}
}

inline constexpr D3D12_COMPARISON_FUNC to_dx12_comparison_func(SRComparisonFunc value) {
	switch (value) {
	case SRComparisonFunc::Never:
		return D3D12_COMPARISON_FUNC_NEVER;
	case SRComparisonFunc::Less:
		return D3D12_COMPARISON_FUNC_LESS;
	case SRComparisonFunc::Equal:
		return D3D12_COMPARISON_FUNC_EQUAL;
	case SRComparisonFunc::LessEqual:
		return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	case SRComparisonFunc::Greater:
		return D3D12_COMPARISON_FUNC_GREATER;
	case SRComparisonFunc::NotEqual:
		return D3D12_COMPARISON_FUNC_NOT_EQUAL;
	case SRComparisonFunc::GreaterEqual:
		return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	case SRComparisonFunc::Always:
		return D3D12_COMPARISON_FUNC_ALWAYS;
	default:
		return D3D12_COMPARISON_FUNC_NEVER;
	}
}

inline constexpr D3D12_CULL_MODE to_dx12_cull_mode(SRCullMode value) {
	switch (value) {
	case SRCullMode::Front:
		return D3D12_CULL_MODE_FRONT;
	case SRCullMode::Back:
		return D3D12_CULL_MODE_BACK;
	default:
		return D3D12_CULL_MODE_NONE;
	}
}

inline constexpr D3D12_DEPTH_WRITE_MASK to_dx12_depth_write_mask(SRDepthWriteMask value) {
	switch (value) {
	case SRDepthWriteMask::All:
		return D3D12_DEPTH_WRITE_MASK_ALL;
	default:
		return D3D12_DEPTH_WRITE_MASK_ZERO;
	}
}

inline constexpr D3D12_FILL_MODE to_dx12_fill_mode(SRFillMode value) {
	switch (value) {
	case SRFillMode::Solid:
		return D3D12_FILL_MODE_SOLID;
	default:
		return D3D12_FILL_MODE_WIREFRAME;
	}
}

inline constexpr D3D12_FILTER to_dx12_filter(SRFilter value) {
	switch (value) {
	case SRFilter::MinMagMipPoint:
		return D3D12_FILTER_MIN_MAG_MIP_POINT;
	case SRFilter::MinMagPointMipLinear:
		return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
	case SRFilter::MinPointMagLinearMipPoint:
		return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
	case SRFilter::MinPointMagMipLinear:
		return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
	case SRFilter::MinLinearMagMipPoint:
		return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
	case SRFilter::MinLinearMagPointMipLinear:
		return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
	case SRFilter::MinMagLinearMipPoint:
		return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	case SRFilter::MinMagMipLinear:
		return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	case SRFilter::Anisotropic:
		return D3D12_FILTER_ANISOTROPIC;
	case SRFilter::ComparisonMinMagMipPoint:
		return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
	case SRFilter::ComparisonMinMagPointMipLinear:
		return D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
	case SRFilter::ComparisonMinPointMagLinearMipPoint:
		return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
	case SRFilter::ComparisonMinPointMagMipLinear:
		return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
	case SRFilter::ComparisonMinLinearMagMipPoint:
		return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
	case SRFilter::ComparisonMinLinearMagPointMipLinear:
		return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
	case SRFilter::ComparisonMinMagLinearMipPoint:
		return D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	case SRFilter::ComparisonMinMagMipLinear:
		return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
	case SRFilter::ComparisonAnisotropic:
		return D3D12_FILTER_COMPARISON_ANISOTROPIC;
	case SRFilter::MinimumMinMagMipPoint:
		return D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT;
	case SRFilter::MinimumMinMagPointMipLinear:
		return D3D12_FILTER_MINIMUM_MIN_MAG_POINT_MIP_LINEAR;
	case SRFilter::MinimumMinPointMagLinearMipPoint:
		return D3D12_FILTER_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT;
	case SRFilter::MinimumMinPointMagMipLinear:
		return D3D12_FILTER_MINIMUM_MIN_POINT_MAG_MIP_LINEAR;
	case SRFilter::MinimumMinLinearMagMipPoint:
		return D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_MIP_POINT;
	case SRFilter::MinimumMinLinearMagPointMipLinear:
		return D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
	case SRFilter::MinimumMinMagLinearMipPoint:
		return D3D12_FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT;
	case SRFilter::MinimumMinMagMipLinear:
		return D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR;
	case SRFilter::MinimumAnisotropic:
		return D3D12_FILTER_MINIMUM_ANISOTROPIC;
	case SRFilter::MaximumMinMagMipPoint:
		return D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT;
	case SRFilter::MaximumMinMagPointMipLinear:
		return D3D12_FILTER_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR;
	case SRFilter::MaximumMinPointMagLinearMipPoint:
		return D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT;
	case SRFilter::MaximumMinPointMagMipLinear:
		return D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR;
	case SRFilter::MaximumMinLinearMagMipPoint:
		return D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT;
	case SRFilter::MaximumMinLinearMagPointMipLinear:
		return D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
	case SRFilter::MaximumMinMagLinearMipPoint:
		return D3D12_FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT;
	case SRFilter::MaximumMinMagMipLinear:
		return D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR;
	case SRFilter::MaximumAnisotropic:
		return D3D12_FILTER_MAXIMUM_ANISOTROPIC;
	default:
		return D3D12_FILTER_MIN_MAG_MIP_POINT;
	}
}

inline constexpr DXGI_FORMAT to_dx12_format(SRFormat format) {
	switch (format) {
	case SRFormat::Unknown:
		return DXGI_FORMAT_UNKNOWN;
	case SRFormat::RGBA32_FLOAT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case SRFormat::RGBA32_UINT:
		return DXGI_FORMAT_R32G32B32A32_UINT;
	case SRFormat::RGBA32_SINT:
		return DXGI_FORMAT_R32G32B32A32_SINT;
	case SRFormat::RGB32_FLOAT:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case SRFormat::RGB32_UINT:
		return DXGI_FORMAT_R32G32B32_UINT;
	case SRFormat::RGB32_SINT:
		return DXGI_FORMAT_R32G32B32_SINT;
	case SRFormat::RGBA16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case SRFormat::RGBA16_UNORM:
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	case SRFormat::RGBA16_UINT:
		return DXGI_FORMAT_R16G16B16A16_UINT;
	case SRFormat::RGBA16_SNORM:
		return DXGI_FORMAT_R16G16B16A16_SNORM;
	case SRFormat::RGBA16_SINT:
		return DXGI_FORMAT_R16G16B16A16_SINT;
	case SRFormat::RG32_FLOAT:
		return DXGI_FORMAT_R32G32_FLOAT;
	case SRFormat::RG32_UINT:
		return DXGI_FORMAT_R32G32_UINT;
	case SRFormat::RG32_SINT:
		return DXGI_FORMAT_R32G32_SINT;
	case SRFormat::D32_FLOAT_S8X24_UINT:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	case SRFormat::RGB10A2_UNORM:
		return DXGI_FORMAT_R10G10B10A2_UNORM;
	case SRFormat::RGB10A2_UINT:
		return DXGI_FORMAT_R10G10B10A2_UINT;
	case SRFormat::RG11B10_FLOAT:
		return DXGI_FORMAT_R11G11B10_FLOAT;
	case SRFormat::RGBA8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case SRFormat::RGBA8_UNORM_SRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case SRFormat::RGBA8_UINT:
		return DXGI_FORMAT_R8G8B8A8_UINT;
	case SRFormat::RGBA8_SNORM:
		return DXGI_FORMAT_R8G8B8A8_SNORM;
	case SRFormat::RGBA8_SINT:
		return DXGI_FORMAT_R8G8B8A8_SINT;
	case SRFormat::RG16_FLOAT:
		return DXGI_FORMAT_R16G16_FLOAT;
	case SRFormat::RG16_UNORM:
		return DXGI_FORMAT_R16G16_UNORM;
	case SRFormat::RG16_UINT:
		return DXGI_FORMAT_R16G16_UINT;
	case SRFormat::RG16_SNORM:
		return DXGI_FORMAT_R16G16_SNORM;
	case SRFormat::RG16_SINT:
		return DXGI_FORMAT_R16G16_SINT;
	case SRFormat::D32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;
	case SRFormat::R32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case SRFormat::R32_UINT:
		return DXGI_FORMAT_R32_UINT;
	case SRFormat::R32_SINT:
		return DXGI_FORMAT_R32_SINT;
	case SRFormat::D24_UNORM_S8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case SRFormat::RGB9E5_SHAREDEXP:
		return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
	case SRFormat::RG8_UNORM:
		return DXGI_FORMAT_R8G8_UNORM;
	case SRFormat::RG8_UINT:
		return DXGI_FORMAT_R8G8_UINT;
	case SRFormat::RG8_SNORM:
		return DXGI_FORMAT_R8G8_SNORM;
	case SRFormat::RG8_SINT:
		return DXGI_FORMAT_R8G8_SINT;
	case SRFormat::R16_FLOAT:
		return DXGI_FORMAT_R16_FLOAT;
	case SRFormat::D16_UNORM:
		return DXGI_FORMAT_D16_UNORM;
	case SRFormat::R16_UNORM:
		return DXGI_FORMAT_R16_UNORM;
	case SRFormat::R16_UINT:
		return DXGI_FORMAT_R16_UINT;
	case SRFormat::R16_SNORM:
		return DXGI_FORMAT_R16_SNORM;
	case SRFormat::R16_SINT:
		return DXGI_FORMAT_R16_SINT;
	case SRFormat::R8_UNORM:
		return DXGI_FORMAT_R8_UNORM;
	case SRFormat::R8_UINT:
		return DXGI_FORMAT_R8_UINT;
	case SRFormat::R8_SNORM:
		return DXGI_FORMAT_R8_SNORM;
	case SRFormat::R8_SINT:
		return DXGI_FORMAT_R8_SINT;
	case SRFormat::BC1_UNORM:
		return DXGI_FORMAT_BC1_UNORM;
	case SRFormat::BC1_UNORM_SRGB:
		return DXGI_FORMAT_BC1_UNORM_SRGB;
	case SRFormat::BC2_UNORM:
		return DXGI_FORMAT_BC2_UNORM;
	case SRFormat::BC2_UNORM_SRGB:
		return DXGI_FORMAT_BC2_UNORM_SRGB;
	case SRFormat::BC3_UNORM:
		return DXGI_FORMAT_BC3_UNORM;
	case SRFormat::BC3_UNORM_SRGB:
		return DXGI_FORMAT_BC3_UNORM_SRGB;
	case SRFormat::BC4_UNORM:
		return DXGI_FORMAT_BC4_UNORM;
	case SRFormat::BC4_SNORM:
		return DXGI_FORMAT_BC4_SNORM;
	case SRFormat::BC5_UNORM:
		return DXGI_FORMAT_BC5_UNORM;
	case SRFormat::BC5_SNORM:
		return DXGI_FORMAT_BC5_SNORM;
	case SRFormat::BGRA8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case SRFormat::BGRA8_UNORM_SRGB:
		return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	case SRFormat::BC6H_UF16:
		return DXGI_FORMAT_BC6H_UF16;
	case SRFormat::BC6H_SF16:
		return DXGI_FORMAT_BC6H_SF16;
	case SRFormat::BC7_UNORM:
		return DXGI_FORMAT_BC7_UNORM;
	case SRFormat::BC7_UNORM_SRGB:
		return DXGI_FORMAT_BC7_UNORM_SRGB;
	case SRFormat::NV12:
		return DXGI_FORMAT_NV12;
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

inline constexpr D3D12_INPUT_CLASSIFICATION to_dx12_input_class(SRInputClass value) {
	switch (value) {
	case SRInputClass::PerInstance:
		return D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
	default:
		return D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	}
}

inline constexpr D3D12_BARRIER_ACCESS to_dx12_access_mask(SRAccessMask value) {
	if (value == SRAccessMask::None) {
		return D3D12_BARRIER_ACCESS_NO_ACCESS;
	}

	D3D12_BARRIER_ACCESS result = D3D12_BARRIER_ACCESS_COMMON;

	if (has_flag(value, SRAccessMask::VertexBuffer)) {
		result |= D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
	}
	if (has_flag(value, SRAccessMask::ConstantBuffer)) {
		result |= D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
	}
	if (has_flag(value, SRAccessMask::IndexBuffer)) {
		result |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
	}
	if (has_flag(value, SRAccessMask::RenderTarget)) {
		result |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
	}
	if (has_flag(value, SRAccessMask::UnorderedAccess)) {
		result |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	}
	if (has_flag(value, SRAccessMask::DepthStencilRead)) {
		result |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
	}
	if (has_flag(value, SRAccessMask::DepthStencilWrite)) {
		result |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
	}
	if (has_flag(value, SRAccessMask::ShaderResource)) {
		result |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
	}
	if (has_flag(value, SRAccessMask::CopySrc)) {
		result |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
	}
	if (has_flag(value, SRAccessMask::CopyDst)) {
		result |= D3D12_BARRIER_ACCESS_COPY_DEST;
	}

	return result;
}

inline constexpr D3D12_BARRIER_LAYOUT to_dx12_resource_state(SRResourceState value) {
	switch (value) {
	case SRResourceState::Undefined:
		return D3D12_BARRIER_LAYOUT_UNDEFINED;
	case SRResourceState::RenderTarget:
		return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
	case SRResourceState::DepthWrite:
		return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
	case SRResourceState::DepthRead:
		return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
	case SRResourceState::ShaderResource:
		return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
	case SRResourceState::UnorderedAccess:
		return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
	default:
		return D3D12_BARRIER_LAYOUT_COMMON;
	}
}

inline constexpr D3D12_BARRIER_SYNC to_dx12_pipeline_stage(SRPipelineStage value) {
	D3D12_BARRIER_SYNC result = D3D12_BARRIER_SYNC_NONE;

	if (has_flag(value, SRPipelineStage::AllCommands)) {
		result |= D3D12_BARRIER_SYNC_ALL;
	}
	if (has_flag(value, SRPipelineStage::Draw)) {
		// TODO: LOOK INTO THIS
		// Invalid for now
		assert(false);
	}
	if (has_flag(value, SRPipelineStage::IndexInput)) {
		assert(false);
	}
	if (has_flag(value, SRPipelineStage::VertexShader)) {
		result |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
	}
	if (has_flag(value, SRPipelineStage::PixelShader)) {
		result |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
	}
	if (has_flag(value, SRPipelineStage::DepthStencil)) {
		// TODO: Investigate
		result |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
	}
	if (has_flag(value, SRPipelineStage::RenderTarget)) {
		result |= D3D12_BARRIER_SYNC_RENDER_TARGET;
	}
	if (has_flag(value, SRPipelineStage::ComputeShader)) {
		result |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	}
	if (has_flag(value, SRPipelineStage::RayTracing)) {
		assert(false);
	}
	if (has_flag(value, SRPipelineStage::Copy)) {
		result |= D3D12_BARRIER_SYNC_COPY;
	}

	return result;
}

inline constexpr D3D12_TEXTURE_ADDRESS_MODE to_dx12_texture_address_mode(SRTextureAddressMode value) {
	switch (value) {
	case SRTextureAddressMode::Wrap:
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	case SRTextureAddressMode::Mirror:
		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	case SRTextureAddressMode::Clamp:
		return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	case SRTextureAddressMode::Border:
		return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	case SRTextureAddressMode::MirrorOnce:
		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
	default:
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}
}
