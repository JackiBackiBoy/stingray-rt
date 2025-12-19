#include "Scene.hpp"

SRScene::SRScene(SRGraphicsDevice& gfxDevice, SREntityID maxEntities) :
	m_GfxDevice(gfxDevice), m_MaxEntities(maxEntities) {

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
