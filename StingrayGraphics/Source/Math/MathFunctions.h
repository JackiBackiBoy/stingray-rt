#pragma once

#include "Core/Types.h"

u32 Rand_XORShift32(u32* state);
f32 Rand_F32(u32* state);
f32 Rand_F32(f32 min, f32 max, u32* state);
