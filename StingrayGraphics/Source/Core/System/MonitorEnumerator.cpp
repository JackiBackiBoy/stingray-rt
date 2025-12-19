#include "MonitorEnumerator.h"
#include <Windows.h>
#include "Utilities/TextUtilities.h"

#include <cassert>
#include <unordered_map>

static inline uint64_t pack_luid(const LUID& l) {
	return ((uint64_t(l.HighPart) << 32) | uint64_t(l.LowPart));
}

struct SRMonitorEnumerator::Impl {
	std::vector<HMONITOR> hMonitors;

	static BOOL monitor_enum_proc(HMONITOR monitor, HDC hdc, LPRECT lpRect, LPARAM lParam);
};

BOOL SRMonitorEnumerator::Impl::monitor_enum_proc(HMONITOR monitor, HDC, LPRECT, LPARAM lParam) {
	Impl* impl = reinterpret_cast<Impl*>(lParam);
	assert(impl);

	MONITORINFOEX monitorInfo = {};
	monitorInfo.cbSize = sizeof(monitorInfo);
	GetMonitorInfo(monitor, &monitorInfo);

	impl->hMonitors.push_back(monitor);

	return TRUE;
}

SRMonitorEnumerator::SRMonitorEnumerator() {
	m_Impl = new Impl();
}

SRMonitorEnumerator::~SRMonitorEnumerator() {
	delete m_Impl;
	m_Impl = nullptr;
}

std::vector<SRMonitorInfo> SRMonitorEnumerator::enumerate() {
	std::vector<SRMonitorInfo> monitors;

	EnumDisplayMonitors(
		nullptr,
		nullptr,
		[](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
			auto monitors = reinterpret_cast<std::vector<SRMonitorInfo>*>(lParam);

			MONITORINFOEX mi = {};
			mi.cbSize = sizeof(mi);

			if (!GetMonitorInfo(hMonitor, &mi)) {
				return TRUE;
			}

			SRMonitorInfo info = {};
			info.width = mi.rcMonitor.right - mi.rcMonitor.left;
			info.height = mi.rcMonitor.bottom - mi.rcMonitor.top;
			info.positionX = mi.rcMonitor.left;
			info.positionY = mi.rcMonitor.top;
			info.gdiName = SRTextUtilities::to_string(mi.szDevice);
			monitors->push_back(info);

			return TRUE;
		},
		reinterpret_cast<LPARAM>(&monitors)
	);

	// 2) Build map DISPLAY# -> friendly name using QueryDisplayConfig
	UINT32 pathCount = 0, modeCount = 0;
	const UINT32 flags = QDC_ONLY_ACTIVE_PATHS;
	if (GetDisplayConfigBufferSizes(flags, &pathCount, &modeCount) != ERROR_SUCCESS) {
		assert(false);
		return {};
	}

	std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
	std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
	if (QueryDisplayConfig(flags, &pathCount, paths.data(), &modeCount, modes.data(), nullptr) != ERROR_SUCCESS) {
		assert(false);
		return {};
	}

	paths.resize(pathCount); modes.resize(modeCount);
	std::unordered_map<std::string, std::string> gdiToFriendly;
	std::unordered_map<std::string, uint64_t> gdiToLUID;

	for (const auto& p : paths) {
		DISPLAYCONFIG_SOURCE_DEVICE_NAME src{};
		src.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
		src.header.size = sizeof(src);
		src.header.adapterId = p.sourceInfo.adapterId;
		src.header.id = p.sourceInfo.id;
		if (DisplayConfigGetDeviceInfo(&src.header) != ERROR_SUCCESS) {
			continue;
		}

		DISPLAYCONFIG_TARGET_DEVICE_NAME tgt{};
		tgt.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
		tgt.header.size = sizeof(tgt);
		tgt.header.adapterId = p.targetInfo.adapterId;
		tgt.header.id = p.targetInfo.id;
		if (DisplayConfigGetDeviceInfo(&tgt.header) != ERROR_SUCCESS) {
			continue;
		}

		std::string gdi = SRTextUtilities::to_string(src.viewGdiDeviceName);
		std::string friendly = "Unknown";

		if (tgt.flags.friendlyNameFromEdid && tgt.monitorFriendlyDeviceName[0] != L'\0') {
			friendly = SRTextUtilities::to_string(tgt.monitorFriendlyDeviceName);
		}

		gdiToFriendly[gdi] = friendly;
		gdiToLUID[gdi] = pack_luid(p.sourceInfo.adapterId);
	}

	for (auto& m : monitors) {
		const auto search = gdiToFriendly.find(m.gdiName);
		if (search != gdiToFriendly.end()) {
			m.friendlyName = search->second;
		}
		else {
			m.friendlyName = m.gdiName;
		}

		if (auto it2 = gdiToLUID.find(m.gdiName); it2 != gdiToLUID.end()) {
			m.adapterLUID = it2->second;
		}
		else {
			m.adapterLUID = 0;
		}
	}

	return monitors;
}
