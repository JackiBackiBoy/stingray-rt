#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class SRCamera {
public:
	SRCamera(
		const glm::vec3& position,
		const glm::quat& orientation,
		float verticalFOV,
		float aspectRatio,
		float zNear,
		float zFar
	);
	~SRCamera() = default;

	void update(); // NOTE: Should ideally be called once per frame
	void set_position(const glm::vec3& position);
	void set_orientation(const glm::quat& orientation);
	void set_vertical_fov(float fov); // NOTE: In degrees
	void set_aspect_ratio(float aspectRatio);

	//SRCameraFrustum get_frustum(float farDistance) const;
	inline glm::vec3 get_position() const { return m_Position; }
	inline glm::quat get_orientation() const { return m_Orientation; }
	inline float get_vertical_fov() const { return m_VerticalFOV; } // NOTE: In degrees
	inline float get_aspect_ratio() const { return m_AspectRatio; }
	inline float get_z_near() const { return m_ZNear; }
	inline float get_z_far() const { return m_ZFar; }
	inline glm::mat4 get_view_matrix() const { return m_ViewMatrix; } // NOTE: Only updated when update() is called
	inline glm::mat4 get_proj_matrix() const { return m_ProjMatrix; } // NOTE: Only updated when update() is called
	inline glm::mat4 get_inv_view_matrix() const { return m_InvViewMatrix; }
	inline glm::mat4 get_inv_proj_matrix() const { return m_InvProjMatrix; }
	inline glm::vec3 get_right() const { return m_Right; }
	inline glm::vec3 get_up() const { return m_Up; }
	inline glm::vec3 get_forward() const { return m_Forward; }

	static constexpr glm::vec3 LH_BASIS_RIGHT = { 1.0f, 0.0f, 0.0f };
	static constexpr glm::vec3 LH_BASIS_UP = { 0.0f, 1.0f, 0.0f };
	static constexpr glm::vec3 LH_BASIS_FORWARD = { 0.0f, 0.0f, 1.0f };
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
	glm::mat4 m_InvViewMatrix = { 1.0f };
	glm::mat4 m_InvProjMatrix = { 1.0f };
	glm::mat4 m_InvViewProjMatrix = { 1.0f };
};
