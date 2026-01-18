#pragma once

#include "Core/Types.h"
#include "Data/ArenaAllocator.h"

constexpr u64 STR8_NOT_FOUND = ~0ull;

struct Str8 {
	u8* data;
	u64 size; // NOTE: Size in bytes (not including null terminating character)
};

struct Str16 {
	u16* data;
	u64 size; // NOTE: Size in bytes (not including null terminating character)
};

Str8  Str8_Build(u8* data, u64 size);
Str8  Str8_Concat(SRArena* arena, Str8 str1, Str8 str2);
u64   Str8_FindFirstOf(Str8 str, u8 c);
u64   Str8_FindLastOf(Str8 str, u8 c);
Str8  Str8_FromStr16(SRArena* arena, Str16 str16);

#define Str8_Literal(S) Str8_Build((u8*)(S), sizeof(S) - 1)

Str16 Str16_Build(u16* data, u64 size);
Str16 Str16_FromStr8(SRArena* arena, Str8 str8);