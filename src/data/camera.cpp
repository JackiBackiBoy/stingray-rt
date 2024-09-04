#include "camera.h"

Camera::Camera(const glm::vec3& position, const glm::quat& orientation, float verticalFOV, float aspectRatio, float zNear, float zFar) :
	m_Position(position), m_Orientation(orientation),
	m_VerticalFOV(verticalFOV), m_AspectRatio(aspectRatio),
	m_ZNear(zNear), m_ZFar(zFar) {

	m_Right = m_Orientation * LH_BASIS_RIGHT;
	m_Up = m_Orientation * LH_BASIS_UP;
	m_Forward = m_Orientation * LH_BASIS_FORWARD;

	update();
}

void Camera::update() noexcept {
	if (m_UpdateViewMatrix) {
		updateViewMatrix();
	}

	if (m_UpdateProjMatrix) {
		updateProjMatrix();
	}

	if (m_UpdateViewMatrix || m_UpdateProjMatrix) {
		m_InvViewProjMatrix = glm::inverse(m_ProjMatrix * m_ViewMatrix);
	}

	m_UpdateViewMatrix = false;
	m_UpdateProjMatrix = false;
}

void Camera::setPosition(const glm::vec3& position) noexcept {
	if (position == m_Position) {
		return;
	}

	m_UpdateViewMatrix = true;
	m_Position = position;
}

void Camera::setOrientation(const glm::quat& orientation) noexcept {
	if (orientation == m_Orientation) {
		return;
	}

	m_UpdateViewMatrix = true;
	m_Orientation = orientation;

	m_Right = glm::normalize(m_Orientation * LH_BASIS_RIGHT);
	m_Up = glm::normalize(m_Orientation * LH_BASIS_UP);
	m_Forward = glm::normalize(m_Orientation * LH_BASIS_FORWARD);
}

void Camera::setVerticalFOV(float fov) noexcept {
	if (fov == m_VerticalFOV) {
		return;
	}

	m_UpdateProjMatrix = true;
	m_VerticalFOV = fov;
}

void Camera::setAspectRatio(float aspectRatio) noexcept {
	if (aspectRatio == m_AspectRatio) {
		return;
	}

	m_UpdateProjMatrix = true;
	m_AspectRatio = aspectRatio;
}

void Camera::updateViewMatrix() {
	m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_Forward, m_Up);
}

void Camera::updateProjMatrix() {
	m_ProjMatrix = glm::perspective(m_VerticalFOV, m_AspectRatio, m_ZNear, m_ZFar);
}
