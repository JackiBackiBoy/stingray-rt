#include "Camera.h"

glm::vec3 SRCamera_GetRight(const SRCamera* camera) {
	return glm::normalize(camera->orientation * SR_LH_BASIS_RIGHT);
}

glm::vec3 SRCamera_GetUp(const SRCamera* camera) {
	return glm::normalize(camera->orientation * SR_LH_BASIS_UP);
}

glm::vec3 SRCamera_GetForward(const SRCamera* camera) {
	return glm::normalize(camera->orientation * SR_LH_BASIS_FORWARD);
}

void SRCamera_ComputeView(const SRCamera* camera, glm::mat4* view) {
	glm::vec3 f = glm::normalize(camera->orientation * SR_LH_BASIS_FORWARD);
	glm::vec3 r = glm::normalize(camera->orientation * SR_LH_BASIS_RIGHT);
	glm::vec3 u = glm::cross(f, r);

	*view = glm::lookAt(camera->position, camera->position + f, u);
}

void SRCamera_ComputeProj(const SRCamera* camera, f32 aspect_ratio, glm::mat4* proj) {
	*proj = glm::perspective(
		glm::radians(camera->vertical_fov),
		aspect_ratio,
		camera->z_far,
		camera->z_near
	);
	// TRICK: Infinite far plane
	(*proj)[2][2] = 0.0f;
	(*proj)[3][2] = camera->z_near;
}
