#pragma once

#include "Core/Logger.hpp"
#include "Graphics/GraphicsTypes.hpp"

#include "d3d12.h"
#include <D3D12MemAlloc.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <Windows.h>

#include <cassert>
#include <cstdint>
#include <stdexcept>

using namespace Microsoft::WRL;

#define SR_DX12_CHECK(expr, msg)                                                 \
	do {                                                                       \
		HRESULT hr = (expr);                                                 \
		if (FAILED(hr)) {                                               \
			SRLOG_ERROR_CAT(SRLOG_CAT_DX12, "%s failed: %s (%d)", msg, "TODO: PARSE ERROR", hr); \
			throw std::runtime_error("DX12 error: " msg);                    \
		}                                                                      \
	} while (0)

class SRDescriptorHeap_DX12 {
public:
	SRDescriptorHeap_DX12(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity);
	~SRDescriptorHeap_DX12() = default;

	void initialize(ID3D12Device* device);

	SRDescriptorIndex get_next_index();
	D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_handle(SRDescriptorIndex index);
	D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_handle(SRDescriptorIndex index);
	ID3D12DescriptorHeap* get_heap_object() const { return m_DescriptorHeap.Get(); }

	void free_index(SRDescriptorIndex index);
	inline uint32_t get_index_from_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle) const {
		return static_cast<uint32_t>((handle.ptr - m_CPUDescriptorHandleStart.ptr) / m_DescriptorSize);
	}

	inline uint32_t get_index_from_handle(D3D12_GPU_DESCRIPTOR_HANDLE handle) const {
		return static_cast<uint32_t>((handle.ptr - m_GPUDescriptorHandleStart.ptr) / m_DescriptorSize);
	}

private:
	inline void clear_state_bit(SRDescriptorIndex index) {
		m_StateArray[index >> 6ull] &= ~(1ull << (index & 63ull));
	}

	inline void set_state_bit(SRDescriptorIndex index) {
		m_StateArray[index >> 6ull] |= (1ull << (index & 63ull));
	}

	inline bool get_state_bit(SRDescriptorIndex index) const {
		return (m_StateArray[index >> 6ull] & (1ull << (index & 63ull))) != 0ull;
	}

	D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
	uint32_t m_Capacity;
	
	uint32_t m_Size = 0;
	uint32_t m_DescriptorSize = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE m_CPUDescriptorHandleStart = {};
	D3D12_GPU_DESCRIPTOR_HANDLE m_GPUDescriptorHandleStart = {};
	ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap;
	std::vector<SRDescriptorIndex> m_FreeList;
	std::vector<uint64_t> m_StateArray;
};

struct SRResource_DX12 {
	virtual ~SRResource_DX12() {
		allocation->Release();
	};

	D3D12MA::Allocation* allocation = nullptr;
};

struct SRBuffer_DX12 : public SRResource_DX12 {

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
	ComPtr<ID3D12GraphicsCommandList7> graphicsCmdList;
};

struct SRPipeline_DX12 {
	ComPtr<ID3D12PipelineState> pipeline;
	ComPtr<ID3D12RootSignature> rootSignature;
};

struct SRSwapchain_DX12 {
	ComPtr<IDXGISwapChain3> swapchain;
	std::vector<ComPtr<ID3D12Resource>> images;
	std::vector<SRDescriptorIndex> rtvDescriptors;
};

// ---------------------------- Converter Functions ----------------------------
inline SRBuffer_DX12* to_dx12_internal(const SRBuffer& buffer) {
	return (SRBuffer_DX12*)buffer.internalState.get();
}

inline SRTexture_DX12* to_dx12_internal(const SRTexture& texture) {
	return (SRTexture_DX12*)texture.internalState.get();
}

inline SRCmdList_DX12* to_dx12_internal(const SRCmdList& cmdList) {
	return (SRCmdList_DX12*)cmdList.internalState;
}

inline SRPipeline_DX12* to_dx12_internal(const SRPipeline& pipeline) {
	return (SRPipeline_DX12*)pipeline.internalState.get();
}

inline SRSwapchain_DX12* to_dx12_internal(const SRSwapchain& swapchain) {
	return (SRSwapchain_DX12*)swapchain.internalState.get();
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
	}
}

inline constexpr D3D12_BLEND to_dx12_blend(SRBlend value) {
	switch (value) {
	case SRBlend::ZERO:
		return D3D12_BLEND_ZERO;
	case SRBlend::ONE:
		return D3D12_BLEND_ONE;
	case SRBlend::SRC_COLOR:
		return D3D12_BLEND_SRC_COLOR;
	case SRBlend::INV_SRC_COLOR:
		return D3D12_BLEND_INV_SRC_COLOR;
	case SRBlend::SRC_ALPHA:
		return D3D12_BLEND_SRC_ALPHA;
	case SRBlend::INV_SRC_ALPHA:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case SRBlend::DEST_ALPHA:
		return D3D12_BLEND_DEST_ALPHA;
	case SRBlend::INV_DEST_ALPHA:
		return D3D12_BLEND_INV_DEST_ALPHA;
	case SRBlend::DEST_COLOR:
		return D3D12_BLEND_DEST_COLOR;
	case SRBlend::INV_DEST_COLOR:
		return D3D12_BLEND_INV_DEST_COLOR;
	case SRBlend::SRC_ALPHA_SAT:
		return D3D12_BLEND_SRC_ALPHA_SAT;
	case SRBlend::BLEND_FACTOR:
		return D3D12_BLEND_BLEND_FACTOR;
	case SRBlend::INV_BLEND_FACTOR:
		return D3D12_BLEND_INV_BLEND_FACTOR;
	case SRBlend::SRC1_COLOR:
		return D3D12_BLEND_SRC1_COLOR;
	case SRBlend::INV_SRC1_COLOR:
		return D3D12_BLEND_INV_SRC1_COLOR;
	case SRBlend::SRC1_ALPHA:
		return D3D12_BLEND_SRC1_ALPHA;
	case SRBlend::INV_SRC1_ALPHA:
		return D3D12_BLEND_INV_SRC1_ALPHA;
	default:
		return D3D12_BLEND_ZERO;
	}
}

inline constexpr D3D12_BLEND to_dx12_alpha_blend(SRBlend value) {
	switch (value) {
	case SRBlend::SRC_COLOR:
		return D3D12_BLEND_SRC_ALPHA;
	case SRBlend::INV_SRC_COLOR:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case SRBlend::DEST_COLOR:
		return D3D12_BLEND_DEST_ALPHA;
	case SRBlend::INV_DEST_COLOR:
		return D3D12_BLEND_INV_DEST_ALPHA;
	case SRBlend::SRC1_COLOR:
		return D3D12_BLEND_SRC1_ALPHA;
	case SRBlend::INV_SRC1_COLOR:
		return D3D12_BLEND_INV_SRC1_ALPHA;
	default:
		return to_dx12_blend(value);
	}
}

inline constexpr D3D12_BLEND_OP to_dx12_blend_op(SRBlendOp value) {
	switch (value) {
	case SRBlendOp::ADD:
		return D3D12_BLEND_OP_ADD;
	case SRBlendOp::SUBTRACT:
		return D3D12_BLEND_OP_SUBTRACT;
	case SRBlendOp::REV_SUBTRACT:
		return D3D12_BLEND_OP_REV_SUBTRACT;
	case SRBlendOp::MIN:
		return D3D12_BLEND_OP_MIN;
	case SRBlendOp::MAX:
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
	case SRComparisonFunc::NEVER:
		return D3D12_COMPARISON_FUNC_NEVER;
	case SRComparisonFunc::LESS:
		return D3D12_COMPARISON_FUNC_LESS;
	case SRComparisonFunc::EQUAL:
		return D3D12_COMPARISON_FUNC_EQUAL;
	case SRComparisonFunc::LESS_EQUAL:
		return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	case SRComparisonFunc::GREATER:
		return D3D12_COMPARISON_FUNC_GREATER;
	case SRComparisonFunc::NOT_EQUAL:
		return D3D12_COMPARISON_FUNC_NOT_EQUAL;
	case SRComparisonFunc::GREATER_EQUAL:
		return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	case SRComparisonFunc::ALWAYS:
		return D3D12_COMPARISON_FUNC_ALWAYS;
	default:
		return D3D12_COMPARISON_FUNC_NEVER;
	}
}

inline constexpr D3D12_CULL_MODE to_dx12_cull_mode(SRCullMode value) {
	switch (value) {
	case SRCullMode::FRONT:
		return D3D12_CULL_MODE_FRONT;
	case SRCullMode::BACK:
		return D3D12_CULL_MODE_BACK;
	default:
		return D3D12_CULL_MODE_NONE;
	}
}

inline constexpr D3D12_DEPTH_WRITE_MASK to_dx12_depth_write_mask(SRDepthWriteMask value) {
	switch (value) {
	case SRDepthWriteMask::ALL:
		return D3D12_DEPTH_WRITE_MASK_ALL;
	default:
		return D3D12_DEPTH_WRITE_MASK_ZERO;
	}
}

inline constexpr D3D12_FILL_MODE to_dx12_fill_mode(SRFillMode value) {
	switch (value) {
	case SRFillMode::SOLID:
		return D3D12_FILL_MODE_SOLID;
	default:
		return D3D12_FILL_MODE_WIREFRAME;
	}
}

inline constexpr D3D12_FILTER to_dx12_filter(SRFilter value) {
	switch (value) {
	case SRFilter::MIN_MAG_MIP_POINT:
		return D3D12_FILTER_MIN_MAG_MIP_POINT;
	case SRFilter::MIN_MAG_POINT_MIP_LINEAR:
		return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
	case SRFilter::MIN_POINT_MAG_LINEAR_MIP_POINT:
		return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
	case SRFilter::MIN_POINT_MAG_MIP_LINEAR:
		return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
	case SRFilter::MIN_LINEAR_MAG_MIP_POINT:
		return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
	case SRFilter::MIN_LINEAR_MAG_POINT_MIP_LINEAR:
		return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
	case SRFilter::MIN_MAG_LINEAR_MIP_POINT:
		return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	case SRFilter::MIN_MAG_MIP_LINEAR:
		return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	case SRFilter::ANISOTROPIC:
		return D3D12_FILTER_ANISOTROPIC;
	case SRFilter::COMPARISON_MIN_MAG_MIP_POINT:
		return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
	case SRFilter::COMPARISON_MIN_MAG_POINT_MIP_LINEAR:
		return D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
	case SRFilter::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:
		return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
	case SRFilter::COMPARISON_MIN_POINT_MAG_MIP_LINEAR:
		return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
	case SRFilter::COMPARISON_MIN_LINEAR_MAG_MIP_POINT:
		return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
	case SRFilter::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
		return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
	case SRFilter::COMPARISON_MIN_MAG_LINEAR_MIP_POINT:
		return D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	case SRFilter::COMPARISON_MIN_MAG_MIP_LINEAR:
		return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
	case SRFilter::COMPARISON_ANISOTROPIC:
		return D3D12_FILTER_COMPARISON_ANISOTROPIC;
	case SRFilter::MINIMUM_MIN_MAG_MIP_POINT:
		return D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT;
	case SRFilter::MINIMUM_MIN_MAG_POINT_MIP_LINEAR:
		return D3D12_FILTER_MINIMUM_MIN_MAG_POINT_MIP_LINEAR;
	case SRFilter::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:
		return D3D12_FILTER_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT;
	case SRFilter::MINIMUM_MIN_POINT_MAG_MIP_LINEAR:
		return D3D12_FILTER_MINIMUM_MIN_POINT_MAG_MIP_LINEAR;
	case SRFilter::MINIMUM_MIN_LINEAR_MAG_MIP_POINT:
		return D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_MIP_POINT;
	case SRFilter::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
		return D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
	case SRFilter::MINIMUM_MIN_MAG_LINEAR_MIP_POINT:
		return D3D12_FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT;
	case SRFilter::MINIMUM_MIN_MAG_MIP_LINEAR:
		return D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR;
	case SRFilter::MINIMUM_ANISOTROPIC:
		return D3D12_FILTER_MINIMUM_ANISOTROPIC;
	case SRFilter::MAXIMUM_MIN_MAG_MIP_POINT:
		return D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT;
	case SRFilter::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR:
		return D3D12_FILTER_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR;
	case SRFilter::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:
		return D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT;
	case SRFilter::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR:
		return D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR;
	case SRFilter::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT:
		return D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT;
	case SRFilter::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
		return D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
	case SRFilter::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT:
		return D3D12_FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT;
	case SRFilter::MAXIMUM_MIN_MAG_MIP_LINEAR:
		return D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR;
	case SRFilter::MAXIMUM_ANISOTROPIC:
		return D3D12_FILTER_MAXIMUM_ANISOTROPIC;
	default:
		return D3D12_FILTER_MIN_MAG_MIP_POINT;
	}
}

inline constexpr DXGI_FORMAT to_dx12_format(SRFormat format) {
	switch (format) {
	case SRFormat::UNKNOWN:
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
	case SRInputClass::PER_INSTANCE:
		return D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
	default:
		return D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	}
}

inline constexpr D3D12_BARRIER_ACCESS to_dx12_access_mask(SRBarrierAccess value) {
	if (value == SRBarrierAccess::None) {
		return D3D12_BARRIER_ACCESS_NO_ACCESS;
	}

	D3D12_BARRIER_ACCESS result = D3D12_BARRIER_ACCESS_COMMON;

	if (has_flag(value, SRBarrierAccess::VertexBuffer)) {
		result |= D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
	}
	if (has_flag(value, SRBarrierAccess::ConstantBuffer)) {
		result |= D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
	}
	if (has_flag(value, SRBarrierAccess::IndexBuffer)) {
		result |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
	}
	if (has_flag(value, SRBarrierAccess::RenderTarget)) {
		result |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
	}
	if (has_flag(value, SRBarrierAccess::UnorderedAccess)) {
		result |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	}
	if (has_flag(value, SRBarrierAccess::DepthStencilRead)) {
		result |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
	}
	if (has_flag(value, SRBarrierAccess::DepthStencilWrite)) {
		result |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
	}
	if (has_flag(value, SRBarrierAccess::ShaderResource)) {
		result |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
	}
	if (has_flag(value, SRBarrierAccess::CopySrc)) {
		result |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
	}
	if (has_flag(value, SRBarrierAccess::CopyDst)) {
		result |= D3D12_BARRIER_ACCESS_COPY_DEST;
	}

	return result;
}

inline constexpr D3D12_BARRIER_LAYOUT to_dx12_resource_state(SRResourceState value) {
	switch (value) {
	case SRResourceState::UNDEFINED:
		return D3D12_BARRIER_LAYOUT_UNDEFINED;
	case SRResourceState::RENDER_TARGET:
		return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
	case SRResourceState::DEPTH_WRITE:
		return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
	case SRResourceState::DEPTH_READ:
		return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
	case SRResourceState::SHADER_RESOURCE:
		return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
	case SRResourceState::UNORDERED_ACCESS:
		return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
	default:
		return D3D12_BARRIER_LAYOUT_COMMON;
	}
}

inline constexpr D3D12_BARRIER_SYNC to_dx12_pipeline_stage(SRBarrierSync value) {
	D3D12_BARRIER_SYNC result = D3D12_BARRIER_SYNC_NONE;

	if (has_flag(value, SRBarrierSync::AllCommands)) {
		result |= D3D12_BARRIER_SYNC_ALL;
	}
	if (has_flag(value, SRBarrierSync::Draw)) {
		// TODO: LOOK INTO THIS
		// Invalid for now
		assert(false);
	}
	if (has_flag(value, SRBarrierSync::IndexInput)) {
		assert(false);
	}
	if (has_flag(value, SRBarrierSync::VertexShader)) {
		result |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
	}
	if (has_flag(value, SRBarrierSync::PixelShader)) {
		result |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
	}
	if (has_flag(value, SRBarrierSync::DepthStencil)) {
		// TODO: Investigate
		result |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
	}
	if (has_flag(value, SRBarrierSync::RenderTarget)) {
		result |= D3D12_BARRIER_SYNC_RENDER_TARGET;
	}
	if (has_flag(value, SRBarrierSync::ComputeShader)) {
		result |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	}
	if (has_flag(value, SRBarrierSync::RayTracing)) {
		assert(false);
	}
	if (has_flag(value, SRBarrierSync::Copy)) {
		result |= D3D12_BARRIER_SYNC_COPY;
	}

	return result;
}

inline constexpr D3D12_TEXTURE_ADDRESS_MODE to_dx12_texture_address_mode(SRTextureAddressMode value) {
	switch (value) {
	case SRTextureAddressMode::WRAP:
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	case SRTextureAddressMode::MIRROR:
		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	case SRTextureAddressMode::CLAMP:
		return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	case SRTextureAddressMode::BORDER:
		return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	case SRTextureAddressMode::MIRROR_ONCE:
		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
	default:
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}
}
