#pragma once

#include "Core/Window.hpp"
#include <cstdint>

typedef uint16_t SRKey;
typedef uint16_t SRMouseButton;
typedef uint64_t SRKeyWord;

static constexpr int SR_KEY_CAP = 512;
static constexpr int SR_KEY_CAP_WORDS = SR_KEY_CAP / 64;

enum SRKey_ : SRKey {
	SRKey_Unknown = 0,

	// Printable keys (Set 1 scancodes, US positions)
	SRKey_A = 0x1E,
	SRKey_B = 0x30,
	SRKey_C = 0x2E,
	SRKey_D = 0x20,
	SRKey_E = 0x12,
	SRKey_F = 0x21,
	SRKey_G = 0x22,
	SRKey_H = 0x23,
	SRKey_I = 0x17,
	SRKey_J = 0x24,
	SRKey_K = 0x25,
	SRKey_L = 0x26,
	SRKey_M = 0x32,
	SRKey_N = 0x31,
	SRKey_O = 0x18,
	SRKey_P = 0x19,
	SRKey_Q = 0x10,
	SRKey_R = 0x13,
	SRKey_S = 0x1F,
	SRKey_T = 0x14,
	SRKey_U = 0x16,
	SRKey_V = 0x2F,
	SRKey_W = 0x11,
	SRKey_X = 0x2D,
	SRKey_Y = 0x15,
	SRKey_Z = 0x2C,

	SRKey_1 = 0x02,
	SRKey_2 = 0x03,
	SRKey_3 = 0x04,
	SRKey_4 = 0x05,
	SRKey_5 = 0x06,
	SRKey_6 = 0x07,
	SRKey_7 = 0x08,
	SRKey_8 = 0x09,
	SRKey_9 = 0x0A,
	SRKey_0 = 0x0B,

	SRKey_Return = 0x1C,
	SRKey_Escape = 0x01,
	SRKey_Backspace = 0x0E,
	SRKey_Tab = 0x0F,
	SRKey_Space = 0x39,
	SRKey_Minus = 0x0C,
	SRKey_Equals = 0x0D,
	SRKey_LeftBracket = 0x1A,
	SRKey_RightBracket = 0x1B,
	SRKey_Backslash = 0x2B,
	SRKey_Semicolon = 0x27,
	SRKey_Apostrophe = 0x28,
	SRKey_Grave = 0x29,
	SRKey_Comma = 0x33,
	SRKey_Period = 0x34,
	SRKey_Slash = 0x35,
	SRKey_ISO_102 = 0x56,

	// Function keys
	SRKey_F1 = 0x3B,
	SRKey_F2 = 0x3C,
	SRKey_F3 = 0x3D,
	SRKey_F4 = 0x3E,
	SRKey_F5 = 0x3F,
	SRKey_F6 = 0x40,
	SRKey_F7 = 0x41,
	SRKey_F8 = 0x42,
	SRKey_F9 = 0x43,
	SRKey_F10 = 0x44,
	SRKey_F11 = 0x57,
	SRKey_F12 = 0x58,

	// Lock keys
	SRKey_CapsLock = 0x3A,
	SRKey_NumLock = 0x45,
	SRKey_ScrollLock = 0x46,

	// Control keys (non-E0)
	SRKey_LeftShift = 0x2A,
	SRKey_RightShift = 0x36,
	SRKey_LeftControl = 0x1D,
	SRKey_LeftAlt = 0x38,

	// E0-prefixed extended keys (use 256 + base)
	SRKey_RightControl = 0x100 | 0x1D,  // E0 1D
	SRKey_RightAlt = 0x100 | 0x38,  // E0 38
	SRKey_Up = 0x100 | 0x48,
	SRKey_Down = 0x100 | 0x50,
	SRKey_Left = 0x100 | 0x4B,
	SRKey_Right = 0x100 | 0x4D,
	SRKey_Home = 0x100 | 0x47,
	SRKey_End = 0x100 | 0x4F,
	SRKey_PageUp = 0x100 | 0x49,
	SRKey_PageDown = 0x100 | 0x51,
	SRKey_Insert = 0x100 | 0x52,
	SRKey_Delete = 0x100 | 0x53,

	SRKey_KP_Divide = 0x100 | 0x35,
	SRKey_KP_Enter = 0x100 | 0x1C,

	// Numpad (non-E0)
	SRKey_KP_Multiply = 0x37,
	SRKey_KP_Subtract = 0x4A,
	SRKey_KP_Add = 0x4E,
	SRKey_KP_0 = 0x52,
	SRKey_KP_1 = 0x4F,
	SRKey_KP_2 = 0x50,
	SRKey_KP_3 = 0x51,
	SRKey_KP_4 = 0x4B,
	SRKey_KP_5 = 0x4C,
	SRKey_KP_6 = 0x4D,
	SRKey_KP_7 = 0x47,
	SRKey_KP_8 = 0x48,
	SRKey_KP_9 = 0x49,
	SRKey_KP_Decimal = 0x53,

	// Windows and menu keys (E0)
	SRKey_LeftSuper = 0x100 | 0x5B,
	SRKey_RightSuper = 0x100 | 0x5C,
	SRKey_Menu = 0x100 | 0x5D,
};

enum SRMouseButton_ : SRMouseButton {
	SRMouseButton_None   = 0,
	SRMouseButton_1      = 1 << 0,
	SRMouseButton_2      = 1 << 1,
	SRMouseButton_3      = 1 << 2,
	SRMouseButton_4      = 1 << 3,
	SRMouseButton_5      = 1 << 4,
	SRMouseButton_6      = 1 << 5,
	SRMouseButton_7      = 1 << 6,
	SRMouseButton_8      = 1 << 7,
	SRMouseButton_9      = 1 << 8,
	SRMouseButton_10     = 1 << 9,
	SRMouseButton_11     = 1 << 10,
	SRMouseButton_12     = 1 << 11,
	SRMouseButton_13     = 1 << 12,
	SRMouseButton_14     = 1 << 13,
	SRMouseButton_15     = 1 << 14,
	SRMouseButton_16     = 1 << 15,

	SRMouseButton_Left   = SRMouseButton_1,
	SRMouseButton_Right  = SRMouseButton_2,
	SRMouseButton_Middle = SRMouseButton_3
};

struct SRKeyboardState {
	// NOTE: Bitset of keys, encoded positions based on USScanCode & 0x1ff
	SRKeyWord down[SR_KEY_CAP_WORDS] = {};
};

struct SRMouseState {
	int dx = 0;
	int dy = 0;
	SRMouseButton buttonStates = SRMouseButton_None;
};

namespace SRInput {
	void initialize(const SRWindow* window); // NOTE: Call ONCE at startup
	void update(); // NOTE: Call ONCE per frame

	SRKeyboardState get_keyboard_state();
	SRMouseState get_mouse_state();
	bool is_key_down(SRKey key);
	bool is_key_up(SRKey key);

	void shutdown(); // NOTE: Call ONCE at shutdown
}
