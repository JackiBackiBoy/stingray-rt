#include "ecs.h"

#include <cassert>
#include <queue>
#include <vector>

namespace ecs {
	template<typename T>
	struct ComponentArray {
		void add(entity_id entity, T component) {
			assert(entityToIndexLUT.find(entity) == entityToIndexLUT.end());

			const size_t newIndex = size;

			entityToIndexLUT.insert({ entity, newIndex });
			indexToEntityLUT.insert({ newIndex, entity });
			components[newIndex] = component;

			++size;
		}

		T* get(entity_id entity) {
			assert(entityToIndexLUT.find(entity) != entityToIndexLUT.end());

			return &components[entityToIndexLUT[entity]];
		}

		T components[MAX_ENTITIES] = {};
		std::unordered_map<entity_id, size_t> entityToIndexLUT = {};
		std::unordered_map<size_t, entity_id> indexToEntityLUT = {};
		size_t size = 0;
	};

	GLOBAL_VARIABLE std::queue<entity_id> g_AvailableEntityIDs = {};
	GLOBAL_VARIABLE uint32_t g_LiveEntityCount = 0;
	GLOBAL_VARIABLE ComponentArray<Transform> g_TransformComponents = {};
	GLOBAL_VARIABLE ComponentArray<Renderable> g_RenderableComponents = {};

	void initialize() {
		for (entity_id i = 0; i < MAX_ENTITIES; ++i) {
			g_AvailableEntityIDs.push(i);
		}
	}

	void destroy() {

	}

	entity_id create_entity() {
		assert(g_LiveEntityCount < MAX_ENTITIES);

		const entity_id id = g_AvailableEntityIDs.front();
		g_AvailableEntityIDs.pop();
		++g_LiveEntityCount;

		// Add default entity components
		add_component(id, Transform{});

		return id;
	}

	void add_component(entity_id entity, const Transform& transform) { g_TransformComponents.add(entity, transform); }
	void add_component(entity_id entity, const Renderable& renderable) { g_RenderableComponents.add(entity, renderable); }

	Transform* get_component_transform(entity_id entity) { return g_TransformComponents.get(entity); }
	Renderable* get_component_renderable(entity_id entity) { return g_RenderableComponents.get(entity); }

}
