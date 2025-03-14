#include "MaterialManager.h"

namespace SR {
	MaterialManager::MaterialManager(GraphicsDevice& gfxDevice, size_t capacity) :
		m_GfxDevice(gfxDevice), m_Capacity(capacity) {
		m_Materials.reserve(m_Capacity);

		// Default material
		Material defaultMaterial = {};
		defaultMaterial.color = { 1.0f, 1.0f, 1.0f };
		defaultMaterial.type = Material::Type::NOT_DIFFUSE_LIGHT;
		defaultMaterial.albedoTexIndex = 0;
		defaultMaterial.normalTexIndex = 1;
		defaultMaterial.metallic = 0.0f;
		defaultMaterial.roughness = 1.0f;
		defaultMaterial.ior = 1.45f;

		add_material(defaultMaterial);

		const BufferInfo bufferInfo = {
			.size = m_Capacity * sizeof(Material),
			.stride = sizeof(Material),
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::SHADER_RESOURCE,
			.miscFlags = MiscFlag::BUFFER_STRUCTURED,
			.persistentMap = true
		};

		m_GfxDevice.create_buffer(bufferInfo, m_MaterialBuffer, nullptr);
	}

	uint32_t MaterialManager::add_material(const Material& material) {
		assert(m_Materials.size() < m_Capacity);

		m_Materials.push_back(material);
		return m_Materials.size() - 1;
	}

	void MaterialManager::update_gpu_buffer() {
		std::memcpy(m_MaterialBuffer.mappedData, m_Materials.data(), m_Materials.size() * sizeof(Material));
	}
}
