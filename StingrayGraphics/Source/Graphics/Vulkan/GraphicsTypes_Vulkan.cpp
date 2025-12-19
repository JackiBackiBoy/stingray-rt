#include "GraphicsTypes_Vulkan.h"

SRDescriptorHeap_Vulkan::SRDescriptorHeap_Vulkan(VkDescriptorType type, u32 capacity) :
	m_Type(type), m_Capacity(capacity) {
	m_StateArray.resize((capacity + 63ull) >> 6ull, 0ull);
}

SRDescriptorIndex SRDescriptorHeap_Vulkan::get_next_index() {
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

void SRDescriptorHeap_Vulkan::free_index(SRDescriptorIndex index) {
	assert(index < m_Size);
	assert(get_state_bit(index) != false);
	clear_state_bit(index);
	m_FreeList.push_back(index);
}
