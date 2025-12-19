#include "Time.h"
#include <Windows.h>

namespace {
	double g_FreqInv = 0.0;
	LONGLONG g_PrevCounter = 0;
	double g_Total = 0.0;
	double g_Delta = 0.0;
}

namespace SRTime {
	void initialize() {
		LARGE_INTEGER freq, now;
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&now);

		g_FreqInv = 1.0 / double(freq.QuadPart);
		g_PrevCounter = now.QuadPart;
		g_Total = 0.0;
		g_Delta = 0.0;
	}

	void begin_frame() {
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		const LONGLONG curr = now.QuadPart;
		g_Delta = (curr - g_PrevCounter) * g_FreqInv;
		g_Total += g_Delta;
		g_PrevCounter = curr;
	}

	double get_delta_sec() {
		return g_Delta;
	}

	double get_elapsed_sec() {
		return g_Total;
	}
}
