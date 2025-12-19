#pragma once

#include <vector>

class SRISparseSet {
public:
	virtual ~SRISparseSet() {}
	virtual void remove(size_t id) = 0;
	virtual size_t get_size() const = 0;
};

template<typename T>
class SRSparseSet : public SRISparseSet {
public:
	SRSparseSet() {
		m_Sparse.reserve(1024);
		m_Dense.reserve(1024);
	}
	~SRSparseSet() {}

	inline T* add(size_t id, T element) {
		m_Sparse.push_back(m_Dense.size());
		m_Dense.push_back(element);
		m_DenseToID.push_back(id);

		return &m_Dense[m_Dense.size() - 1];
	}

	inline T* get(size_t id) {
		if (id >= m_Sparse.size()) {
			return nullptr;
		}

		const size_t index = m_Sparse[id];
		if (index == NULL_INDEX) {
			return nullptr;
		}

		return &m_Dense[index];
	}

	inline void remove(size_t id) override {
		const size_t deletedIndex = get_dense_index(id);

		if (m_Dense.empty() || deletedIndex == NULL_INDEX) {
			return;
		}

		m_Sparse[m_DenseToID.back()] = deletedIndex;
		m_Sparse[id] = NULL_INDEX;

		std::swap(m_Dense.back(), m_Dense[deletedIndex]);
		std::swap(m_DenseToID.back(), m_DenseToID[deletedIndex]);

		m_Dense.pop_back();
		m_DenseToID.pop_back();
	}

	inline size_t get_size() const override { return m_Dense.size(); }
	inline const std::vector<T>& get_data() const { return m_Dense; }

private:
	static constexpr size_t NULL_INDEX = ~0ULL;

	inline size_t get_dense_index(size_t id) {
		return m_Sparse[id];
	}

	std::vector<size_t> m_Sparse;
	std::vector<T> m_Dense;
	std::vector<size_t> m_DenseToID;
};
