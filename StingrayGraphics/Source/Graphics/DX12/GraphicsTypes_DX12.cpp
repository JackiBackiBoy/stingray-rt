#include "GraphicsTypes_DX12.h"

SRDescriptorHeap_DX12* SRDescriptorHeap_DX12_Create(SRArena* arena, ID3D12Device* d3d12Device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 capacity) {
	SRDescriptorHeap_DX12* heap = SRArena_PushStructZero(arena, SRDescriptorHeap_DX12);
	heap->capacity = capacity;
	heap->heapType = type;
	heap->descriptorHandleSize = d3d12Device->GetDescriptorHandleIncrementSize(type);

	D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags;
	if (type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV) {
		heapFlags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	}
	else {
		heapFlags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
		.Type = type,
		.NumDescriptors = capacity,
		.Flags = heapFlags,
		.NodeMask = 0
	};

	HR(d3d12Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap->heapObject)));
	heap->cpuDescriptorHandleStart = heap->heapObject->GetCPUDescriptorHandleForHeapStart();
	if (type != D3D12_DESCRIPTOR_HEAP_TYPE_RTV && type != D3D12_DESCRIPTOR_HEAP_TYPE_DSV) {
		heap->gpuDescriptorHandleStart = heap->heapObject->GetGPUDescriptorHandleForHeapStart();
	}

	return heap;
}

SRDescriptorIndex SRDescriptorHeap_DX12_GetNextIndex(SRDescriptorHeap_DX12* heap) {
	assert(heap->count < heap->capacity);
	return heap->count++;
}

D3D12_CPU_DESCRIPTOR_HANDLE SRDescriptorHeap_DX12_GetCPUHandle(SRDescriptorHeap_DX12* heap, SRDescriptorIndex index) {
	assert(index < heap->count);
	return { heap->cpuDescriptorHandleStart.ptr + (u64)index * heap->descriptorHandleSize };
}

D3D12_GPU_DESCRIPTOR_HANDLE SRDescriptorHeap_DX12_GetGPUHandle(SRDescriptorHeap_DX12* heap, SRDescriptorIndex index) {
	assert(index < heap->count);
	return { heap->gpuDescriptorHandleStart.ptr + u64(index) * heap->descriptorHandleSize };
}

SRDescriptorIndex SRDescriptorHeap_DX12_GetIndexFromCPUHandle(SRDescriptorHeap_DX12* heap, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) {
	assert(cpuHandle.ptr >= heap->cpuDescriptorHandleStart.ptr);
	return  (SRDescriptorIndex)((cpuHandle.ptr - heap->cpuDescriptorHandleStart.ptr) / heap->descriptorHandleSize);
}

SRDescriptorIndex SRDescriptorHeap_DX12_GetIndexFromGPUHandle(SRDescriptorHeap_DX12* heap, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) {
	assert(gpuHandle.ptr >= heap->gpuDescriptorHandleStart.ptr);
	return (SRDescriptorIndex)((gpuHandle.ptr - heap->gpuDescriptorHandleStart.ptr) / heap->descriptorHandleSize);
}

void SRDescriptorHeap_DX12_Destroy(SRDescriptorHeap_DX12* heap) {
	heap->heapObject->Release();
	heap->heapObject = nullptr;
}

SRDestructionHandler_DX12* SRDestructionHandler_DX12_Create(SRArena* arena) {
	SRDestructionHandler_DX12* handler = SRArena_PushStructZero(arena, SRDestructionHandler_DX12);
	assert(handler);

	handler->objects = SRArray_Create(arena, 1024, sizeof(IUnknown*));
	return handler;
}

void SRDestructionhandler_DX12_Update(SRDestructionHandler_DX12* handler, u64 frameCount, u64 frameExpiration) {
	for (u64 i = 0; i < handler->objects->count; ++i) {
		IUnknown* obj = &((IUnknown*)handler->objects->data)[i];
		obj->Release();
	}

	SRArray_Clear(handler->objects);
}

void SRDestructionHandler_DX12_Enqueue(SRDestructionHandler_DX12* handler, IUnknown* obj) {
	// TODO: Dynamic resize perhaps??
	SRArray_Append(handler->objects, IUnknown*, obj);
}

void SRDestructionHandler_DX12_Destroy(SRDestructionHandler_DX12* handler) {
	SRDestructionhandler_DX12_Update(handler, ~0u, 0);
	// TODO: Add to free list or something

}

