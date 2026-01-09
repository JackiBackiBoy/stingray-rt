#pragma once

#include "Core/Types.h"
#include <assert.h>
#include <stdlib.h>

#define SR_ARENA_DEFAULT_ALIGNMENT 8 

struct SRArena {
	u8* data;
	u8* committed;
	u8* allocated;
};

struct SRArenaMarker {
	u8* pos;
};

SRArena* SRArena_Create(u64 capacity);
void SRArena_Destroy(SRArena* arena);

void* SRArena_Push(SRArena* arena, u64 size, u64 align);
void* SRArena_PushZero(SRArena* arena, u64 size, u64 align);
#define SRArena_PushBytes(arena, size)            (u8*)SRArena_Push((arena), (size), SR_ARENA_DEFAULT_ALIGNMENT)
#define SRArena_PushBytesZero(arena, size)        (u8*)SRArena_PushZero((arena), (size), SR_ARENA_DEFAULT_ALIGNMENT)
#define SRArena_PushStruct(arena, type)           (type*)SRArena_Push((arena), sizeof(type), alignof(type))
#define SRArena_PushStructZero(arena, type)       (type*)SRArena_PushZero((arena), sizeof(type), alignof(type))
#define SRArena_PushArray(arena, type, count)     (type*)SRArena_Push((arena), (count) * sizeof(type), alignof(type))
#define SRArena_PushArrayZero(arena, type, count) (type*)SRArena_PushZero((arena), (count) * sizeof(type), alignof(type))

void SRArena_Pop(SRArena* arena, u64 size);
#define SRArena_PopStruct(arena, type)       SRArena_Pop((arena), sizeof(type))
#define SRArena_PopArray(arena, type, count) SRArena_Pop((arena), (count) * sizeof(type))

SRArenaMarker SRArena_GetMarker(SRArena* arena);
void SRArena_PopToMarker(SRArena* arena, SRArenaMarker marker);

void SRArena_Clear(SRArena* arena);

// TODO: Move to its own TU
struct SRArray {
	void* data;
	u64 capacity;
	u64 count;
	SRArena* backing_arena;
};

SRArray* SRArray_Create(SRArena* arena, u64 capacity, u64 item_size);
void SRArray_Destroy(SRArray* array);

void SRArray_Clear(SRArray* array);

#define SRArray_Append(array, type, value) \
	do {\
		assert((array)->count < (array)->capacity);\
		((type*)(array)->data)[(array)->count++] = (value);\
	} while(0)

template<typename T>
struct SRVector {
	T* data;
	u64 capacity;
	u64 size;

	T& operator [](u64 i) {
		assert(i < size);
		return data[i];
	}
};

template<typename T>
void SRVector_Create(SRVector<T>* vec) {
	vec->capacity = 16;
	vec->data = (T*)malloc(vec->capacity * sizeof(T));
	vec->size = 0;
}

template<typename T>
void SRVector_PushBack(SRVector<T>* vec, T obj) {
	if (vec->size >= vec->capacity) {
		vec->capacity = vec->capacity * 2;

		T* new_data = (T*)realloc(vec->data, vec->capacity * sizeof(T));
		assert(new_data);

		vec->data = new_data;
	}

	vec->data[vec->size++] = obj;
}

template<typename T>
void SRVector_PopBack(SRVector<T>* vec) {
	assert(vec->size > 0);

	// TODO: Look into calling realloc for reducing capacity, right now we never reduce capacity
	vec->size--;
}

template<typename T>
void SRVector_Destroy(SRVector<T>* vec) {
	free(vec->data);
	vec->data = nullptr;
	vec->capacity = 0;
	vec->size = 0;
}
