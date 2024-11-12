#pragma once

#include "../ecs/ecs.h"
#include "../graphics/gfx_device.h"

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct PointLight {
	glm::vec4 color = {}; // NOTE: w-component is intensity
	glm::vec3 position = {};
	uint32_t pad1 = 0;
};

class Scene {
public:
	Scene(GFXDevice& gfxDevice);
	~Scene();

	entity_id add_entity(const std::string& name);

	inline const std::vector<entity_id>& get_entities() const { return m_Entities; }
	inline glm::vec3 get_sun_color() const { return m_SunColor; }
	inline glm::vec3 get_sun_direction() const { return m_SunDirection; }

	inline void set_sun_color(const glm::vec3& col) { m_SunColor = col; }
	inline void set_sun_direction(const glm::vec3& dir) { m_SunDirection = glm::normalize(dir); }

	glm::vec3 m_SunDirection = { 0.0f, 1.0f, 0.0f };

	static constexpr int MAX_POINT_LIGHTS = 32;
private:
	GFXDevice& m_GfxDevice;

	std::unordered_map<std::string, size_t> m_EntityIndicesMap = {};
	std::vector<entity_id> m_Entities = {};

	glm::vec3 m_SunColor = { 1.0f, 1.0f, 1.0f };
};