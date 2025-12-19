#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct SRMonitorInfo {
	uint32_t width;
	uint32_t height;
	uint32_t positionX;
	uint32_t positionY;
	uint64_t adapterLUID;
	std::string gdiName;
	std::string friendlyName;
};

class SRMonitorEnumerator {
public:
	SRMonitorEnumerator();
	~SRMonitorEnumerator();

	std::vector<SRMonitorInfo> enumerate();

private:
	struct Impl;
	Impl* m_Impl;
};