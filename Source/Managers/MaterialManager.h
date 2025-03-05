#pragma once

#include "ECS/Components.h"
#include "Graphics/GraphicsDevice.h"

#include <cstdint>
#include <glm/glm.hpp>

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