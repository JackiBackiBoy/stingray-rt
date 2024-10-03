#pragma once

#include "gfx_device.h"
#include <glm/glm.hpp>

class Renderer3D {
public:
	Renderer3D(GFXDevice& device);
	~Renderer3D() {};

	void draw_quad(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3);

private:
	GFXDevice& m_Device;
};