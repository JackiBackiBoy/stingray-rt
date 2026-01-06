#pragma once

#include "Core/Types.h"
#include <assert.h>

#define SR_ARENA_DEFAULT_ALIGNMENT 8 

struct SRArena {
	u8* data;
	u8* committed;
	u8* allocated;
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
