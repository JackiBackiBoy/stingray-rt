#include "Input.h"
#include "Core/Logger.h"

#include <cassert>
#include <vector>
#include <Windows.h>

namespace {
	static constexpr UINT RI_SCRATCH_BUFFER_CAPACITY = 64;

	const SRWindow* g_Window = nullptr;
	HWND g_LastActiveHwnd = nullptr;
	std::vector<u8> g_RawInputScratchBuffer;
	bool g_Initialized = false;

	SRKeyboardState g_FrameKeyboardState = {};
	SRMouseState g_FrameMouseState = {};

	inline int key_index(SRKey key) { return key & 0x1FF; }

	inline void keyboard_set_down(SRKeyboardState& ks, u16 key) {
		const int i = key_index(key);
		ks.down[i >> 6] |= (SRKeyWord(1) << (i & 63));
	}

	inline void keyboard_set_up(SRKeyboardState& ks, u16 key) {
		const int i = key_index(key);
		ks.down[i >> 6] &= ~(SRKeyWord(1) << (i & 63));
	}

	inline bool keyboard_is_down(const SRKeyboardState& ks, u16 key) {
		const int i = key_index(key);
		return (ks.down[i >> 6] >> (i & 63)) & 1u;
	}
}

namespace SRInput {
	void initialize(const SRWindow* window) {
		assert(window && !g_Initialized);
		g_Window = window;

		HWND hWnd = reinterpret_cast<HWND>(window->get_internal_handle());
		RAWINPUTDEVICE rids[2] = {};

		// Mouse
		rids[0].usUsagePage = 0x01; // Generic desktop controls
		rids[0].usUsage = 0x02; // Mouse
		rids[0].dwFlags = 0;
		rids[0].hwndTarget = hWnd;

		// Keyboard
		rids[1].usUsagePage = 0x01; // Generic desktop controls
		rids[1].usUsage = 0x06; // Keyboard
		rids[1].dwFlags = 0;
		rids[1].hwndTarget = hWnd;

		BOOL res = RegisterRawInputDevices(rids, 2, sizeof(RAWINPUTDEVICE));
		assert(res == TRUE);

		g_RawInputScratchBuffer.resize(RI_SCRATCH_BUFFER_CAPACITY * sizeof(RAWINPUT));
		g_Initialized = true;
	}

	bool is_mouse_button_down(SRMouseButton buttonStates) {
		return (g_FrameMouseState.buttonStates & buttonStates);
	}

	void update() {
		HWND activeHwnd = GetForegroundWindow();

		if (activeHwnd != g_LastActiveHwnd && activeHwnd != g_Window->get_internal_handle()) {
			std::memset(g_FrameKeyboardState.down, 0, SR_KEY_CAP_WORDS * sizeof(SRKeyWord));
			g_FrameMouseState.buttonStates = SRMouseButton_None;
		}

		if (activeHwnd != g_LastActiveHwnd) {
			g_LastActiveHwnd = activeHwnd;
		}

		g_FrameMouseState.dx = 0;
		g_FrameMouseState.dy = 0;

		const UINT riHeaderSize = sizeof(RAWINPUTHEADER);

		UINT processed = 0;
		UINT iterations = 0;
		for (;;) {
			RAWINPUT* riBuffer = reinterpret_cast<RAWINPUT*>(g_RawInputScratchBuffer.data());
			UINT scratchBufferSize = static_cast<UINT>(g_RawInputScratchBuffer.size());
			UINT n = GetRawInputBuffer(riBuffer, &scratchBufferSize, riHeaderSize);

			if (n == 0) {
				break;
			}
			if (n == (UINT)-1) {
				const DWORD error = GetLastError();
				SRLOG_ERROR("GetRawInputBuffer failed. Error: %lu", error);
				assert(false);
			}

			processed += n;
			++iterations;

			u8* riBytePtr = g_RawInputScratchBuffer.data();
			for (UINT i = 0; i < n; ++i) {
				RAWINPUT* ri = reinterpret_cast<RAWINPUT*>(riBytePtr);

				switch (ri->header.dwType) {
				case RIM_TYPEMOUSE:
				{
					const RAWMOUSE& rawMouse = ri->data.mouse;
					const USHORT flags = rawMouse.usFlags;
					const USHORT buttonFlags = rawMouse.usButtonFlags;

					if (flags & MOUSE_MOVE_ABSOLUTE) {
						continue;
					}

					g_FrameMouseState.dx += static_cast<int>(rawMouse.lLastX);
					g_FrameMouseState.dy += static_cast<int>(rawMouse.lLastY);

					if (buttonFlags & RI_MOUSE_BUTTON_1_DOWN) { g_FrameMouseState.buttonStates |=  SRMouseButton_1; }
					if (buttonFlags & RI_MOUSE_BUTTON_1_UP)   { g_FrameMouseState.buttonStates &= ~SRMouseButton_1; }
					if (buttonFlags & RI_MOUSE_BUTTON_2_DOWN) { g_FrameMouseState.buttonStates |=  SRMouseButton_2; }
					if (buttonFlags & RI_MOUSE_BUTTON_2_UP)   { g_FrameMouseState.buttonStates &= ~SRMouseButton_2; }
					if (buttonFlags & RI_MOUSE_BUTTON_3_DOWN) { g_FrameMouseState.buttonStates |=  SRMouseButton_3; }
					if (buttonFlags & RI_MOUSE_BUTTON_3_UP)   { g_FrameMouseState.buttonStates &= ~SRMouseButton_3; }
					if (buttonFlags & RI_MOUSE_BUTTON_4_DOWN) { g_FrameMouseState.buttonStates |=  SRMouseButton_4; }
					if (buttonFlags & RI_MOUSE_BUTTON_4_UP)   { g_FrameMouseState.buttonStates &= ~SRMouseButton_4; }
					if (buttonFlags & RI_MOUSE_BUTTON_5_DOWN) { g_FrameMouseState.buttonStates |=  SRMouseButton_5; }
					if (buttonFlags & RI_MOUSE_BUTTON_5_UP)   { g_FrameMouseState.buttonStates &= ~SRMouseButton_5; }
				}
				break;
				case RIM_TYPEKEYBOARD:
				{
					const RAWKEYBOARD& rawKeyboard = ri->data.keyboard;
					const USHORT flags = rawKeyboard.Flags;

					if (rawKeyboard.MakeCode == KEYBOARD_OVERRUN_MAKE_CODE || rawKeyboard.VKey >= UCHAR_MAX) {
						continue;
					}

					u16 key = 0;
					if (rawKeyboard.MakeCode) {
						key = rawKeyboard.MakeCode & 0xff;
					}
					else {
						continue;
					}

					if (flags & RI_KEY_E1) {
						continue;
					}

					if (flags & RI_KEY_E0) {
						key |= 0x100;
					}

					const bool isRelease = (flags & RI_KEY_BREAK) != 0;
					if (isRelease) {
						keyboard_set_up(g_FrameKeyboardState, key);
					}
					else {
						keyboard_set_down(g_FrameKeyboardState, key);
					}
				}
				break;
				}

				riBytePtr += ri->header.dwSize;
			}
		}

		//SRLOG_TRACE("Processed %u raw messages with %u iterations", processed, iterations);
	}

	SRKeyboardState get_keyboard_state() {
		return g_FrameKeyboardState;
	}

	SRMouseState get_mouse_state() {
		return g_FrameMouseState;
	}

	bool is_key_down(SRKey key) {
		return keyboard_is_down(g_FrameKeyboardState, key);
	}

	bool is_key_up(SRKey key) {
		return !is_key_down(key);
	}

	void shutdown() {
		
	}
}
