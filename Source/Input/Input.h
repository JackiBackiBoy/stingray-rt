#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace SR {
	enum class GamepadButton : uint8_t {
		Menu,
		View,
		A,
		B,
		X,
		Y,
		DPadUp,
		DPadDown,
		DPadLeft,
		DPadRight,
		LeftShoulder,
		RightShoulder,
		LeftThumbstick,
		RightThumbstick,

		COUNT
	};

	enum class Key : uint16_t {
		UNKNOWN = 0,

		/* Printable keys */
		SPACE = 32,
		APOSTROPHE = 39,  /* ' */
		COMMA = 44,  /* , */
		MINUS = 45,  /* - */
		PERIOD = 46,  /* . */
		SLASH = 47,  /* / */
		ALPHA0 = 48,
		ALPHA1 = 49,
		ALPHA2 = 50,
		ALPHA3 = 51,
		ALPHA4 = 52,
		ALPHA5 = 53,
		ALPHA6 = 54,
		ALPHA7 = 55,
		ALPHA8 = 56,
		ALPHA9 = 57,
		SEMICOLON = 59, /* ; */
		EQUAL = 61, /* = */
		A = 65,
		B = 66,
		C = 67,
		D = 68,
		E = 69,
		F = 70,
		G = 71,
		H = 72,
		I = 73,
		J = 74,
		K = 75,
		L = 76,
		M = 77,
		N = 78,
		O = 79,
		P = 80,
		Q = 81,
		R = 82,
		S = 83,
		T = 84,
		U = 85,
		V = 86,
		W = 87,
		X = 88,
		Y = 89,
		Z = 90,
		LEFT_BRACKET = 91, /* [ */
		BACKSLASH = 92,  /* \ */
		RIGHT_BRACKET = 93, /* ] */
		GRAVE_ACCENT = 96,/* ` */
		WORLD_1 = 161, /* non-US #1 */
		WORLD_2 = 162, /* non-US #2 */

		/* Function keys */
		ESCAPE = 256,
		ENTER = 257,
		TAB = 258,
		BACKSPACE = 259,
		INSERT = 260,
		DEL = 261,
		RIGHT = 262,
		LEFT = 263,
		DOWN = 264,
		UP = 265,
		PAGE_UP = 266,
		PAGE_DOWN = 267,
		HOME = 268,
		END = 269,
		CAPS_LOCK = 280,
		SCROLL_LOCK = 281,
		NUM_LOCK = 282,
		PRINT_SCREEN = 283,
		PAUSE = 284,
		F1 = 290,
		F2 = 291,
		F3 = 292,
		F4 = 293,
		F5 = 294,
		F6 = 295,
		F7 = 296,
		F8 = 297,
		F9 = 298,
		F10 = 299,
		F11 = 300,
		F12 = 301,
		F13 = 302,
		F14 = 303,
		F15 = 304,
		F16 = 305,
		F17 = 306,
		F18 = 307,
		F19 = 308,
		F20 = 309,
		F21 = 310,
		F22 = 311,
		F23 = 312,
		F24 = 313,
		F25 = 314,
		KP_0 = 320,
		KP_1 = 321,
		KP_2 = 322,
		KP_3 = 323,
		KP_4 = 324,
		KP_5 = 325,
		KP_6 = 326,
		KP_7 = 327,
		KP_8 = 328,
		KP_9 = 329,
		KP_DECIMAL = 330,
		KP_DIVIDE = 331,
		KP_MULTIPLY = 332,
		KP_SUBTRACT = 333,
		KP_ADD = 334,
		KP_ENTER = 335,
		KP_EQUAL = 336,
		LEFT_SHIFT = 340,
		LEFT_CONTROL = 341,
		LEFT_ALT = 342,
		LEFT_SUPER = 343,
		RIGHT_SHIFT = 344,
		RIGHT_CONTROL = 345,
		RIGHT_ALT = 346,
		RIGHT_SUPER = 347,
		MENU = 348,

		LAST = MENU
	};

	enum class MouseButton : uint8_t {
		MOUSE1,
		MOUSE2,
		MOUSE3,
		MOUSE4,
		MOUSE5,
		BUTTON_COUNT,

		LEFT = MOUSE1,
		RIGHT = MOUSE2,
		MIDDLE = MOUSE3,
	};

	enum class ButtonAction : uint8_t {
		NONE,
		PRESS,
		RELEASE
	};

	enum class ButtonMods : uint8_t {
		NONE,
	};

	struct GamepadState {
		bool buttons[(int)GamepadButton::COUNT] = { false };
		bool isAnyButtonDown = false;
		float leftTrigger = 0.0f;
		float rightTrigger = 0.0f;
		glm::vec2 leftThumbstick = { 0.0f, 0.0f };
		glm::vec2 rightThumbstick = { 0.0f, 0.0f };
	};

	struct KeyboardState {
		bool buttons[(int)Key::LAST + 1] = { false };
		bool isAnyKeyDown = false;
	};

	struct MouseState {
		glm::ivec2 position = { 0, 0 };
		glm::ivec2 delta = { 0, 0 };
		bool buttons[(int)MouseButton::BUTTON_COUNT];
	};

	namespace Input::Internal {
		void update_key_state(Key key, bool pressed); // NOTE: Called internally by A10 Engine
		void update_mouse_position(glm::ivec2 pos); // NOTE: Called internally by A10 Engine
		void update_mouse_buttons(bool mouse1, bool mouse2, bool mouse3);
	}

	namespace Input {
		void initialize(); // NOTE: Call once at start-up
		void update(); // NOTE: Should only be called once per frame
		void shutdown(); // NOTE: Call once at shutdown

		bool is_down(Key key);
		bool is_down(MouseButton button);
		bool is_down(GamepadButton button);
		bool is_any_key_down();
		bool is_gamepad_active();
		bool is_gamepad_axes_active();

		const MouseState& get_mouse_state();
		const GamepadState& get_gamepad_state();
		glm::ivec2 get_mouse_position();
		glm::ivec2 get_mouse_delta();
		void set_thumbstick_deadzone(float deadzone);
	}
}