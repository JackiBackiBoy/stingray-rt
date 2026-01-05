#pragma once

#include "Core/Types.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

inline constexpr glm::vec3 SR_LH_BASIS_RIGHT   = { 1.0f, 0.0f, 0.0f };
inline constexpr glm::vec3 SR_LH_BASIS_UP      = { 0.0f, 1.0f, 0.0f };
inline constexpr glm::vec3 SR_LH_BASIS_FORWARD = { 0.0f, 0.0f, 1.0f };

struct SRCamera {
	glm::vec3 position;
	glm::quat orientation;
	f32 vertical_fov;
	f32 z_near;
	f32 z_far;
};

glm::vec3 SRCamera_GetRight(const SRCamera* camera);
glm::vec3 SRCamera_GetUp(const SRCamera* camera);
glm::vec3 SRCamera_GetForward(const SRCamera* camera);
void      SRCamera_ComputeView(const SRCamera* camera, glm::mat4* view);
void      SRCamera_ComputeProj(const SRCamera* camera, f32 aspect_ratio, glm::mat4* proj);
