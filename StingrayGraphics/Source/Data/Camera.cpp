#include "Camera.h"

SRCamera::SRCamera(
	const glm::vec3& position,
	const glm::quat& orientation,
	float verticalFOV,
	float aspectRatio,
	float zNear,
	float zFar
) :
	m_Position(position), m_Orientation(orientation),
	m_VerticalFOV(verticalFOV), m_AspectRatio(aspectRatio),
	m_ZNear(zNear), m_ZFar(zFar)
{
	m_Right = m_Orientation * LH_BASIS_RIGHT;
	m_Up = m_Orientation * LH_BASIS_UP;
	m_Forward = m_Orientation * LH_BASIS_FORWARD;

	update();
}

void SRCamera::update() {
	if (m_UpdateViewMatrix) {
		update_view_matrix();
		m_InvViewMatrix = glm::inverse(m_ViewMatrix);
		m_UpdateViewMatrix = false;
	}

	if (m_UpdateProjMatrix) {
		update_proj_matrix();
		m_InvProjMatrix = glm::inverse(m_ProjMatrix);
		m_UpdateProjMatrix = false;
	}
}

void SRCamera::set_position(const glm::vec3& position) {
	if (position == m_Position) {
		return;
	}

	m_Position = position;
	m_UpdateViewMatrix = true;
}

void SRCamera::set_orientation(const glm::quat& orientation) {
	if (orientation == m_Orientation) {
		return;
	}

	m_Orientation = orientation;
	m_Right = glm::normalize(m_Orientation * LH_BASIS_RIGHT);
	m_Up = glm::normalize(m_Orientation * LH_BASIS_UP);
	m_Forward = glm::normalize(m_Orientation * LH_BASIS_FORWARD);
	m_UpdateViewMatrix = true;
}

void SRCamera::set_vertical_fov(float fov) {
	if (fov == m_VerticalFOV) {
		return;
	}

	m_VerticalFOV = fov;
	m_UpdateProjMatrix = true;
}

void SRCamera::set_aspect_ratio(float aspectRatio) {
	if (aspectRatio == m_AspectRatio) {
		return;
	}

	m_AspectRatio = aspectRatio;
	m_UpdateProjMatrix = true;
}

void SRCamera::update_view_matrix() {
	m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_Forward, m_Up);
}

void SRCamera::update_proj_matrix() {
	m_ProjMatrix = glm::perspective(glm::radians(m_VerticalFOV), m_AspectRatio, m_ZFar, m_ZNear);
	// TRICK: Infinite far plane
	m_ProjMatrix[2][2] = 0.0f;
	m_ProjMatrix[3][2] = m_ZNear;

}
