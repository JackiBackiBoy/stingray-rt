#pragma once

#include "Core/Types.h"

#include <string>
#include <vector>

struct SRMonitorInfo {
	u32 width;
	u32 height;
	u32 positionX;
	u32 positionY;
	u64 adapterLUID;
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