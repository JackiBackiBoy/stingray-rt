#include "GraphicsTypes_DX12.hpp"

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
