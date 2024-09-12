#pragma once

#include "components.h"
#include "../platform.h"

namespace ecs {
	void initialize();
	void destroy();

	entity_id create_entity();
	void add_component(entity_id entity, const Transform& transform);
	void add_component(entity_id entity, const Renderable& renderable);

	Transform* get_component_transform(entity_id entity);
	Renderable* get_component_renderable(entity_id entity);

	GLOBAL_VARIABLE constexpr entity_id MAX_ENTITIES = 16384;
}
