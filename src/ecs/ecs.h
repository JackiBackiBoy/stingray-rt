#pragma once

#include "components.h"
#include "../platform.h"

namespace ecs {
	void initialize();
	void destroy();

	template <typename T>
	void add_component(entity_id entity, const T& component);

	template <typename T>
	T* get_component(entity_id entity);

	entity_id create_entity();

	GLOBAL constexpr entity_id MAX_ENTITIES = 16384;
}
