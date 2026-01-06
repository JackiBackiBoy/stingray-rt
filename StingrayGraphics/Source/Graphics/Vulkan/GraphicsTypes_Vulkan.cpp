#include "GraphicsTypes_Vulkan.h"
#include <assert.h>

SRDescriptorHeap_Vulkan* SRDescriptorHeap_Vulkan_Create(SRArena* arena, VkDescriptorType type, u32 capacity) {
	SRDescriptorHeap_Vulkan* heap = SRArena_PushStructZero(arena, SRDescriptorHeap_Vulkan);
	heap->capacity = capacity;
	heap->type = type;
	SRVector_Create(&heap->free_list);

	return heap;
}

SRDescriptorIndex SRDescriptorHeap_Vulkan_GetNextIndex(SRDescriptorHeap_Vulkan* heap) {
	if (heap->free_list.size > 0) {
		SRDescriptorIndex index = heap->free_list[heap->free_list.size - 1];
		SRVector_PopBack(&heap->free_list);
		return index;
	}

	assert(heap->count < heap->capacity);
	return ++heap->count;
}

void SRDescriptorHeap_Vulkan_FreeIndex(SRDescriptorHeap_Vulkan* heap, SRDescriptorIndex index) {
	assert(index > 0 && index <= heap->count);
	SRVector_PushBack(&heap->free_list, index);
}

void SRDescriptorHeap_Vulkan_Destroy(SRDescriptorHeap_Vulkan* heap) {
	SRVector_Destroy(&heap->free_list);
}
