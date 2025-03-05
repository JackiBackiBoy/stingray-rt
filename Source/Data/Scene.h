#pragma once

#include "ECS/ECS.h"
#include "Graphics/GraphicsDevice.h"

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

class Scene {
public:
	Scene(const std::string& name, GFXDevice& gfxDevice);
	~Scene();

	entity_id add_entity(const std::string& name);

	inline const std::string& get_name() const { return m_Name; }
	inline const std::vector<entity_id>& get_entities() const { return m_Entities; }
private:
	std::string m_Name;
	GFXDevice& m_GfxDevice;

	std::unordered_map<std::string, size_t> m_EntityIndicesMap = {};
	std::vector<entity_id> m_Entities = {};
};