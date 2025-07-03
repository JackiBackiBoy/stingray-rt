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
			if (entityToIndexLUT.find(entity) == entityToIndexLUT.end()) {
				return nullptr;
			}

			return &components[entityToIndexLUT[entity]];
		}

		T components[MAX_ENTITIES] = {};
		std::unordered_map<entity_id, size_t> entityToIndexLUT = {};
		std::unordered_map<size_t, entity_id> indexToEntityLUT = {};
		size_t size = 0;
	};

	GLOBAL std::queue<entity_id> g_AvailableEntityIDs = {};
	GLOBAL uint32_t g_LiveEntityCount = 0;
	GLOBAL ComponentArray<Transform> g_TransformComponents = {};
	GLOBAL ComponentArray<Renderable> g_RenderableComponents = {};
	GLOBAL ComponentArray<Material> g_MaterialComponents = {};

	void initialize() {
		for (entity_id i = 0; i < MAX_ENTITIES; ++i) {
			g_AvailableEntityIDs.push(i);
		}
	}

	void destroy() {

	}

	template <typename T>
	void add_component(entity_id entity, const T& component) {
		static_assert(sizeof(T) == 0, "add_component is not implemented for this component type.");
	}

	template <>
	void add_component<Transform>(entity_id entity, const Transform& component) {
		g_TransformComponents.add(entity, component);
	}

	template <>
	void add_component<Renderable>(entity_id entity, const Renderable& component) {
		g_RenderableComponents.add(entity, component);
	}

	template <>
	void add_component<Material>(entity_id entity, const Material& component) {
		g_MaterialComponents.add(entity, component);
	}

	template <typename T>
	T* get_component(entity_id entity) {
		static_assert(sizeof(T) == 0, "get_component is not implemented for this component type.");
	}

	template <>
	Transform* get_component<Transform>(entity_id entity) {
		return g_TransformComponents.get(entity);
	}

	template <>
	Renderable* get_component<Renderable>(entity_id entity) {
		return g_RenderableComponents.get(entity);
	}

	template <>
	Material* get_component<Material>(entity_id entity) {
		return g_MaterialComponents.get(entity);
	}

	template <>
	bool has_component<Material>(entity_id entity) {
		return g_MaterialComponents.get(entity) != nullptr;
	}

	entity_id create_entity() {
		assert(g_LiveEntityCount < MAX_ENTITIES);

		const entity_id id = g_AvailableEntityIDs.front();
		g_AvailableEntityIDs.pop();
		++g_LiveEntityCount;

		// Add default entity components
		add_component<Transform>(id, Transform{});

		return id;
	}

}
