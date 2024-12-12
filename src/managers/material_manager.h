#pragma once

#include "../graphics/gfx_device.h"

#include <cstdint>
#include <glm/glm.hpp>

struct Material {
	enum Type : uint32_t {
		NOT_DIFFUSE_LIGHT = 0,
		DIFFUSE_LIGHT = 1,
	};

	glm::vec3 color = { 1.0f, 1.0f, 1.0f };
	uint32_t type = Type::NOT_DIFFUSE_LIGHT;
	uint32_t albedoTexIndex = 0;
	uint32_t normalTexIndex = 1;
	float metallic = 0.0f; // 0 = dielectric, 1 = metallic
	float roughness = 1.0f;
	float ior = 1.45f;
};

// TODO: There's a lot of optimizations that can be made here, and a lot of
// potential improvements in terms of updating the material buffer.
class MaterialManager {
public:
	MaterialManager(GFXDevice& gfxDevice, size_t capacity);
	~MaterialManager() {}

	uint32_t add_material(const Material& material);
	void update_gpu_buffer();

	inline const std::vector<Material>& get_materials() const { return m_Materials; }
	inline const Buffer& get_material_buffer() const { return m_MaterialBuffer; }

private:
	GFXDevice& m_GfxDevice;
	size_t m_Capacity;

	std::vector<Material> m_Materials = {};
	Buffer m_MaterialBuffer = {};
};