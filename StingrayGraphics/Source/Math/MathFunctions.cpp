#include "MathFunctions.h"

u32 Rand_XORShift32(u32* state) {
	u32 x = *state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*state = x;
	return x;
}

f32 Rand_F32(u32* state) {
	return (f32)Rand_XORShift32(state) / 4294967296.0f;
}

f32 Rand_F32(f32 min, f32 max, u32* state) {
	return min + (max - min) * Rand_F32(state);
}
