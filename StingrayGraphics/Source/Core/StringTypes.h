#pragma once

#include "Core/Types.h"
#include "Data/ArenaAllocator.h"

struct Str8 {
	u8* data;
	u64 size; // NOTE: Size in bytes (not including null terminating character)
};

struct Str16 {
	u16* data;
	u64 size; // NOTE: Size in bytes (not including null terminating character)
};

void Str8_ToStr16(const Str8* str8, Str16* str16);
void  Str16_ToStr8(const Str16* str16, Str8* str8);

Str8  Str8_Build(u8* data, u64 size);
Str8  Str8_Concat(SRArena* arena, Str8 str1, Str8 str2);
Str8  Str8_FromStr16(SRArena* arena, Str16 str16);

Str16 Str16_Build(u16* data, u64 size);
Str16 Str16_FromStr8(SRArena* arena, Str8 str8);

#define Str8_Literal(S) Str8_Build((u8*)(S), sizeof(S) - 1)