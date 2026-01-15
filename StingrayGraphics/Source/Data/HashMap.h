#pragma once

#include "Core/Types.h"
#include "Data/ArenaAllocator.h"

#include <assert.h>
#include <stdlib.h>

#define SR_HASHMAP_MAX_LOAD_FACTOR 0.5f

template<typename K, typename V >
struct SRHashMap {
	struct Slot {
		K key;
		V val;
		bool occupied;
	};

	Slot* slots;
	u64 size;
	u64 capacity;

	u64 (*hash_func)(const K* key);
	bool (*equals_func)(const K* key1, const K* key2);
};

template<typename K, typename V>
void SRHashMap_Create(SRHashMap<K, V>* hash_map, u64(*hash_func)(const K* key), bool (*equals_func)(const K* key1, const K* key2)) {
	hash_map->capacity = 64;
	hash_map->slots = (typename SRHashMap<K, V>::Slot*)calloc(
		hash_map->capacity,
		sizeof(typename SRHashMap<K, V>::Slot)
	);
	hash_map->size = 0;
	hash_map->hash_func = hash_func;
	hash_map->equals_func = equals_func;
}

template<typename K, typename V>
void SRHashMap_Create(SRHashMap<K, V>* hash_map, u64(*hash_func)(const K*)) {
	struct Default {
		static bool Equals(const K* a, const K* b) {
			return *a == *b;
		}
	};

	SRHashMap_Create(hash_map, hash_func, &Default::Equals);
}

template<typename K, typename V>
V* SRHashMap_Get(SRHashMap<K, V>* hash_map, K key) {
	// TODO: Get rid of modulo
	u64 hash = hash_map->hash_func(&key);
	u64 mask = hash_map->capacity - 1;
	u64 index = hash & mask;
	u64 start_index = index;

	while (hash_map->slots[index].occupied) {
		if (hash_map->equals_func(&hash_map->slots[index].key, &key)) {
			return &hash_map->slots[index].val;
		}

		index = (index + 1) & mask; // Faster wrap-around
		if (index == start_index) break;
	}

	return nullptr;
}

template<typename K, typename V>
void SRHashMap_Put(SRHashMap<K, V>* hash_map, K key, V val) {
	// TODO: Get rid of modulo

	if ((hash_map->size + 1) >= (u64)(hash_map->capacity * SR_HASHMAP_MAX_LOAD_FACTOR)) {
		u64 old_capacity = hash_map->capacity;
		u64 new_capacity = old_capacity * 2;

		auto* old_data = hash_map->slots;
		auto* new_data = (typename SRHashMap<K, V>::Slot*)calloc(
			new_capacity,
			sizeof(typename SRHashMap<K, V>::Slot)
		);

		// Rehashing
		hash_map->size = 0;
		for (u64 i = 0; i < old_capacity; ++i) {
			auto* old_slot = &old_data[i];

			if (!old_slot->occupied) {
				continue;
			}

			u64 hash = hash_map->hash_func(&old_slot->key);
			u64 index = hash % new_capacity;
			auto* new_slot = &new_data[index];

			while (new_slot->occupied) {
				index = (index + 1 < new_capacity) ? index + 1 : 0;
				new_slot = &new_data[index];
			}

			new_slot->key = old_slot->key;
			new_slot->val = old_slot->val;
			new_slot->occupied = true;
			hash_map->size++;
		}

		hash_map->slots = new_data;
		hash_map->capacity = new_capacity;

		free(old_data);
	}

	u64 hash = hash_map->hash_func(&key);
	u64 index = hash % hash_map->capacity;
	auto* slot = &hash_map->slots[index];

	while(slot->occupied) {
		if (hash_map->equals_func(&slot->key, &key)) {
			slot->val = val;
			return;
		}

		index = (index + 1 < hash_map->capacity) ? index + 1 : 0;
		slot = &hash_map->slots[index];
	}

	slot->key = key;
	slot->val = val;
	slot->occupied = true;
	hash_map->size++;
}

template<typename K, typename V>
void SRHashMap_Destroy(SRHashMap<K, V>* hash_map) {
	assert(hash_map->slots);
	free(hash_map->slots);
	hash_map->slots = nullptr;
}
