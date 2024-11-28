#pragma once

#include "../data/model.h"
#include <glm/glm.hpp>
#include "glm/gtc/quaternion.hpp"

#include <cstdint>

using entity_id = uint32_t;

struct Transform {
	glm::quat orientation = { 1.0f, 0.0f, 0.0f, 0.0f };
	glm::vec3 position = { 0.0f, 0.0f, 0.0f };
	glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
};

struct Renderable {
	const Model* model = nullptr;
};

struct Material {
	enum Type : uint32_t {
		LAMBERTIAN = 0,
		DIFFUSE_LIGHT = 1,
		METAL = 2,
	};

	glm::vec3 color = { 1.0f, 1.0f, 1.0f };
	uint32_t type = Type::LAMBERTIAN;
	float roughness = 1.0f;
};