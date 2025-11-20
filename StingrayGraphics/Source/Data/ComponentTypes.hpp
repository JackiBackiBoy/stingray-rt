#pragma once

#include "Data/Model.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct SRRenderable {
	const SRModel* model = nullptr;
	glm::vec4 color = glm::vec4(1.0f);
};

struct SRTransform {
	glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
	float scale = 1.0f;
	glm::quat orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
};
