#include "Scene.h"

#include <cassert>

namespace SR {
	Scene::Scene(const std::string& name, GraphicsDevice& gfxDevice) :
		m_Name(name), m_GfxDevice(gfxDevice) {

	}

	Scene::~Scene() {

	}

	entity_id Scene::add_entity(const std::string& name) {
		assert(m_EntityIndicesMap.find(name) == m_EntityIndicesMap.end());

		const entity_id entity = ECS::create_entity();

		m_EntityIndicesMap.insert({ name, m_Entities.size() });
		m_Entities.push_back(entity);

		return entity;
	}
}
