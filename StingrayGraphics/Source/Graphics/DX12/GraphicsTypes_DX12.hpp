#pragma once

#include "Core/Logger.hpp"
#include "Graphics/GraphicsTypes.hpp"

#include "AgilitySDK/d3d12.h"
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

	void free_index(SRDescriptorIndex index);

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

SRDescriptorHeap_DX12::SRDescriptorHeap_DX12(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity) :
	m_Type(type), m_Capacity(capacity) {
	m_StateArray.resize((capacity + 63ull) >> 6ull, 0ull);
}

void SRDescriptorHeap_DX12::initialize(ID3D12Device* device) {
	D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags;
	if (m_Type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || m_Type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV) {
		heapFlags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	}
	else {
		heapFlags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	}

	const D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
		.Type = m_Type,
		.NumDescriptors = m_Capacity,
		.Flags = heapFlags,
		.NodeMask = 0
	};

	SR_DX12_CHECK(device->CreateDescriptorHeap(
		&heapDesc,
		IID_PPV_ARGS(&m_DescriptorHeap)
	), "Descriptor heap creation");
	m_DescriptorSize = device->GetDescriptorHandleIncrementSize(m_Type);
	m_CPUDescriptorHandleStart = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	if (m_Type != D3D12_DESCRIPTOR_HEAP_TYPE_RTV && m_Type != D3D12_DESCRIPTOR_HEAP_TYPE_DSV) {
		m_GPUDescriptorHandleStart = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	}
}

SRDescriptorIndex SRDescriptorHeap_DX12::get_next_index() {
	if (!m_FreeList.empty()) {
		const SRDescriptorIndex index = m_FreeList.back();
		m_FreeList.pop_back();
		set_state_bit(index);
		return index;
	}

	assert(m_Size < m_Capacity);
	set_state_bit(m_Size);
	return m_Size++;
}

D3D12_CPU_DESCRIPTOR_HANDLE SRDescriptorHeap_DX12::get_cpu_handle(SRDescriptorIndex index) {
	assert(index < m_Size && get_state_bit(index) == true);
	return { m_CPUDescriptorHandleStart.ptr + uint64_t(index) * m_DescriptorSize };
}

D3D12_GPU_DESCRIPTOR_HANDLE SRDescriptorHeap_DX12::get_gpu_handle(SRDescriptorIndex index) {
	assert(m_Type != D3D12_DESCRIPTOR_HEAP_TYPE_RTV && m_Type != D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	assert(index < m_Size && get_state_bit(index) == true);
	return { m_GPUDescriptorHandleStart.ptr + uint64_t(index) * m_DescriptorSize };
}

void SRDescriptorHeap_DX12::free_index(SRDescriptorIndex index) {
	assert(index < m_Size);
	assert(get_state_bit(index) != false);
	clear_state_bit(index);
	m_FreeList.push_back(index);
}

struct SRCmdList_DX12 {
	ComPtr<ID3D12GraphicsCommandList7> graphicsCmdList;
};

struct SRSwapchain_DX12 {
	ComPtr<IDXGISwapChain3> swapchain;
	std::vector<ComPtr<ID3D12Resource>> images;
	std::vector<SRDescriptorIndex> rtvDescriptors;
};

// ---------------------------- Converter Functions ----------------------------
inline SRCmdList_DX12* to_internal(const SRCmdList& cmdList) {
	return (SRCmdList_DX12*)cmdList.internalState;
}

inline SRSwapchain_DX12* to_internal(const SRSwapchain& swapchain) {
	return (SRSwapchain_DX12*)swapchain.internalState.get();
}

inline constexpr D3D12_COMMAND_LIST_TYPE to_dx12_cmd_list_type(SRQueue queue) {
	switch (queue) {
	case SRQueue_Universal:
		return D3D12_COMMAND_LIST_TYPE_DIRECT;
	case SRQueue_Compute:
		return D3D12_COMMAND_LIST_TYPE_COMPUTE;
	default:
		return D3D12_COMMAND_LIST_TYPE_NONE;
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
	return DXGI_FORMAT_UNKNOWN;
}
