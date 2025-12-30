#include "ArenaAllocator.h"
#include <stdlib.h>
#include <assert.h>
#include <Windows.h>

// TODO: If we ever want Linux support, we need to use mmap here

SRArena* SRArena_Create() {
	SRArena* arena = (SRArena*)malloc(sizeof(SRArena));
	assert(arena);
	ZeroMemory(arena, sizeof(SRArena));

	SIZE_T reserve_size = 1ull << 33; // ~8 GiB of virtual address space to be reserved
	arena->data = (u8*)VirtualAlloc(nullptr, reserve_size, MEM_RESERVE, PAGE_NOACCESS);
	assert(arena->data);
	arena->allocated = arena->data;
	arena->committed = arena->data;

	return arena;
}

void SRArena_Destroy(SRArena* arena) {
	assert(arena);
	assert(arena->data);

	BOOL res = VirtualFree(arena->data, 0, MEM_RELEASE);
	assert(res != 0);
	free(arena);
}

void* SRArena_Push(SRArena* arena, u64 size, u64 align) {
	assert(arena);
	assert((align & (align - 1)) == 0);

	uintptr_t ptr = (uintptr_t)arena->allocated;
	uintptr_t ptr_aligned = (ptr + align - 1) & ~(uintptr_t)(align - 1);
	u8* new_ptr = (u8*)ptr_aligned;

	arena->allocated = new_ptr + size;

	// TODO: Add check if we ever exceed reserved virtual address space (unlikely, but should still check)
	if (arena->committed < arena->allocated) {
		SYSTEM_INFO system_info;
		GetSystemInfo(&system_info);

		u64 commit_size = arena->allocated - arena->committed;
		commit_size += system_info.dwPageSize - 1;
		commit_size -= commit_size % system_info.dwPageSize;

		void* res = VirtualAlloc(arena->committed, commit_size, MEM_COMMIT, PAGE_READWRITE);
		assert(res);
		arena->committed = (u8*)res + commit_size;
	}

	return new_ptr;
}

void* SRArena_PushZero(SRArena* arena, u64 size, u64 align) {
	void* ptr = SRArena_Push(arena, size, align);
	ZeroMemory(ptr, size);
	return ptr;
}

void SRArena_Pop(SRArena* arena, u64 size) {
	assert(size <= (uintptr_t)(arena->allocated - arena->data));
	arena->allocated -= size;
}

void SRArena_Clear(SRArena* arena) {
	arena->allocated = arena->data;
}

SRArray* SRArray_Create(SRArena* arena, u64 capacity, u64 item_size) {
	SRArray* array = SRArena_PushStruct(arena, SRArray);
	array->data = SRArena_Push(arena, capacity * item_size, SR_ARENA_DEFAULT_ALIGNMENT);
	array->capacity = capacity;
	array->count = 0;
	array->backing_arena = arena;

	return array;
}

void SRArray_Destroy(SRArray* array) {

}

void SRArray_Clear(SRArray* array) {
	array->count = 0;
}
