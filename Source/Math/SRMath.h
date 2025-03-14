#pragma once

#include <cassert>
#include <cstdint>

namespace SR::Math {
	// NOTE: Assumes alignment to be power of 2
	inline uint32_t align_to(uint32_t value, uint32_t alignment) {
		//return ((value + alignment - 1) / alignment) * alignment;
		return (value + alignment - 1) & ~(alignment - 1);
	}
}