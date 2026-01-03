#include "GraphicsTypes_Vulkan.h"
#include <assert.h>

SRDescriptorHeap_Vulkan* SRDescriptorHeap_Vulkan_Create(SRArena* arena, VkDescriptorType type, u32 capacity) {
	SRDescriptorHeap_Vulkan* heap = SRArena_PushStructZero(arena, SRDescriptorHeap_Vulkan);
	heap->capacity = capacity;
	heap->type = type;

	return heap;
}

SRDescriptorIndex SRDescriptorHeap_Vulkan_GetNextIndex(SRDescriptorHeap_Vulkan* heap) {
	assert(heap->count < heap->capacity);
	return heap->count++;
}

void SRDescriptorHeap_Vulkan_Destroy(SRDescriptorHeap_Vulkan* heap) {

}
