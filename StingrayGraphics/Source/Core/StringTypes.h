#pragma once

#include "Core/Types.h"

struct Str8 {
	u8* data;
	u64 size;
};

struct Str16 {
	u16* data;
	u64 size;
};

void Str8_ToStr16(const Str8* str8, Str16* str16);

void Str16_ToStr8(const Str16* str16, Str8* str8);
