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