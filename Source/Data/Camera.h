#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Camera {
public:
	Camera(
		const glm::vec3& position,
		const glm::quat& orientation,
		float verticalFOV,
		float aspectRatio,
		float zNear,
		float zFar
	);
	~Camera() = default;

	void update() noexcept; // NOTE: Should ideally be called once per frame

	void set_position(const glm::vec3& position) noexcept;
	void set_orientation(const glm::quat& orientation) noexcept;
	void set_vertical_fov(float fov) noexcept; // NOTE: In degrees
	void set_aspect_ratio(float aspectRatio) noexcept;

	inline glm::vec3 get_position() const noexcept { return m_Position; }
	inline glm::quat get_orientation() const noexcept { return m_Orientation; }
	inline float get_vertical_fov() const noexcept { return m_VerticalFOV; } // NOTE: In degrees
	inline float get_aspect_ratio() const noexcept { return m_AspectRatio; }
	inline float get_z_near() const noexcept { return m_ZNear; }
	inline float get_z_far() const noexcept { return m_ZFar; }
	inline glm::mat4 get_view_matrix() const noexcept { return m_ViewMatrix; } // NOTE: Only updated when update() is called
	inline glm::mat4 get_proj_matrix() const noexcept { return m_ProjMatrix; } // NOTE: Only updated when update() is called
	inline glm::mat4 get_inv_view_proj_matrix() const noexcept { return m_InvViewProjMatrix; } // NOTE: Only updated when update() is called
	inline glm::vec3 get_right() const noexcept { return m_Right; }
	inline glm::vec3 get_up() const noexcept { return m_Up; }
	inline glm::vec3 get_forward() const noexcept { return m_Forward; }

private:
	void update_view_matrix();
	void update_proj_matrix();

	bool m_UpdateViewMatrix = true;
	bool m_UpdateProjMatrix = true;

	glm::vec3 m_Position;
	glm::quat m_Orientation;
	float m_VerticalFOV; // NOTE: In degrees
	float m_AspectRatio;
	float m_ZNear;
	float m_ZFar;

	glm::vec3 m_Right = { 1.0f, 0.0f, 0.0f };
	glm::vec3 m_Up = { 0.0f, 1.0f, 0.0f };
	glm::vec3 m_Forward = { 0.0f, 0.0f, 1.0f };

	glm::mat4 m_ViewMatrix = { 1.0f };
	glm::mat4 m_ProjMatrix = { 1.0f };
	glm::mat4 m_InvViewProjMatrix = { 1.0f };

	static constexpr glm::vec3 LH_BASIS_RIGHT = { 1.0f, 0.0f, 0.0f };
	static constexpr glm::vec3 LH_BASIS_UP = { 0.0f, 1.0f, 1.0f };
	static constexpr glm::vec3 LH_BASIS_FORWARD = { 0.0f, 0.0f, 1.0f };
};