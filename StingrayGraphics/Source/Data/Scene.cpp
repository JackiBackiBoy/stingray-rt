#include "Scene.h"

SRScene::SRScene(SRGFXDevice& gfx_device, SREntityID maxEntities) :
	m_GfxDevice(gfx_device), m_MaxEntities(maxEntities) {

}

SRScene::~SRScene() {
	for (auto* componentArray : m_ComponentArrays) {
		delete componentArray;
	}
}

SREntityID SRScene::add_entity(const char* name /*= nullptr*/) {
	(void)name;

	assert(m_EntityCount < m_MaxEntities);
	const SREntityID id = m_EntityCount++;
	m_Entities.push_back(id);

	return id;
}
