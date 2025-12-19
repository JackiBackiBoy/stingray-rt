#pragma once

#include "Data/SparseSet.hpp"
#include "Graphics/GraphicsDevice.hpp"

#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>

typedef uint32_t SREntityID;
typedef uint32_t SRComponentID;
typedef uint64_t SRComponentSignature; // Bitset based on 1 << componentID

class SRScene {
public:
	SRScene(SRGraphicsDevice& gfxDevice, SREntityID maxEntities);
	~SRScene();

	SREntityID add_entity(const char* name = nullptr);

	template<typename T>
	T* add_component(SREntityID entity, T component) {
		// TODO: Protection against adding to the same entity multiple times
		const SRComponentID componentID = get_component_id<T>();
		SRComponentSignature* const pComponentSignature = m_EntityComponentSignatures.get(entity);

		if (componentID >= m_ComponentArrays.size()) {
			m_ComponentArrays.push_back(new SRSparseSet<T>());
		}

		if (pComponentSignature == nullptr) { // entity not associated with any signature
			m_EntityComponentSignatures.add(entity, (1ull << componentID));

			const auto newGroupSearch = m_Groups.find((1ull << componentID));
			if (newGroupSearch == m_Groups.end()) { // no group for the signature exists, so create one
				m_Groups.insert({ (1ull << componentID), SRSparseSet<SREntityID>() });
				m_Groups[(1ull << componentID)].add(entity, entity);
			}
			else {
				newGroupSearch->second.add(entity, entity);
			}
		}
		else {
			m_Groups[*pComponentSignature].remove(entity);
			*pComponentSignature |= (1ull << componentID);

			const auto newGroupSearch = m_Groups.find(*pComponentSignature);
			if (newGroupSearch == m_Groups.end()) { // no group for the signature exists, so create one
				m_Groups.insert({ *pComponentSignature, SRSparseSet<SREntityID>() });
				m_Groups[*pComponentSignature].add(entity, entity);
			}
			else {
				newGroupSearch->second.add(entity, entity);
			}
		}

		return reinterpret_cast<SRSparseSet<T>*>(m_ComponentArrays[componentID])->add(entity, component);
	}

	template <typename T>
	T* get_component(SREntityID entity) {
		const SRComponentID componentID = get_component_id<T>();

		if (componentID >= m_ComponentArrays.size()) {
			return nullptr;
		}

		return reinterpret_cast<SRSparseSet<T>*>(m_ComponentArrays[componentID])->get(entity);
	}

	// NOTE: This function only allows iteration over ONE component type,
	// and should thus be preferred over for_each in that case.
	// This is because you get direct access to the contiguous array.
	template <typename T>
	const std::vector<T>& get_all_components() {
		const SRComponentID componentID = get_component_id<T>();

		return reinterpret_cast<SRSparseSet<T>*>(m_ComponentArrays[componentID])->get_data();
	}

	template <typename... Components, typename Func>
	void for_each(Func&& forEachFunc) {
		const SRComponentSignature targetSignature = (
			0 | ... | (1 << get_component_id<Components>())
		);

		for (const auto& [signature, entities] : m_Groups) {
			if ((targetSignature & signature) == targetSignature) {
				for (SREntityID entity : entities.get_data()) {
					std::tuple<Components&...> components = {
						*get_component<Components>(entity)...
					};

					std::apply(forEachFunc, components);
				}
			}
		}
	}

private:
	static constexpr SRComponentID MAX_COMPONENTS = 8 * sizeof(SRComponentSignature);

	template <typename T>
	inline SRComponentID get_component_id() {
		static SRComponentID componentID = g_ComponentIDCounter++;
		assert(componentID < MAX_COMPONENTS);
		return componentID;
	}
	inline static SRComponentID g_ComponentIDCounter = 0;

	SRGraphicsDevice& m_GfxDevice;
	SREntityID m_MaxEntities;
	SREntityID m_EntityCount = 0;
	std::vector<SREntityID> m_Entities;
	std::vector<SRISparseSet*> m_ComponentArrays;
	std::unordered_map<SRComponentSignature, SRSparseSet<SREntityID>> m_Groups;
	SRSparseSet<SRComponentSignature> m_EntityComponentSignatures;
};
