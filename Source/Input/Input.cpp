#include "Input.h"
#include "Core/Platform.h"

#include <Windows.h>
//#include "GameInput.h"

#include <cassert>
#include <optional>
#include <vector>

namespace SR {
	enum class InputDevice {
		KEYBOARD_AND_MOUSE,
		GAMEPAD
	};

	GLOBAL KeyboardState g_WorkingKeyboard = {};
	GLOBAL KeyboardState g_CurrentKeyboard = {};
	GLOBAL MouseState g_WorkingMouse = {};
	GLOBAL MouseState g_CurrentMouse = {};
	GLOBAL GamepadState g_CurrentGamepad = {};

	GLOBAL std::optional<glm::ivec2> g_LastMousePos = {};
	GLOBAL std::optional<glm::ivec2> g_CurrentMousePos = {};
	//GLOBAL IGameInput* g_GameInput = nullptr;
	//GLOBAL IGameInputDevice* g_GamePad = nullptr;
	//GLOBAL GameInputCallbackToken g_CallbackToken;
	GLOBAL InputDevice g_LastInputDevice = InputDevice::KEYBOARD_AND_MOUSE;

	INTERNAL inline void apply_deadzone(glm::vec2& stick, float deadzone) {
		const float magnitude = glm::length(stick);

		if (magnitude < deadzone) {
			stick.x = 0.0f;
			stick.y = 0.0f;
			return;
		}

		stick = glm::normalize(stick) * ((magnitude - deadzone) / (1.0f - deadzone));
	}

	namespace Input::Internal {
		void update_key_state(Key key, bool pressed) {
			g_WorkingKeyboard.buttons[(int)key] = pressed;
		}

		void update_mouse_position(glm::ivec2 pos) {
			g_LastMousePos = g_CurrentMousePos;
			g_CurrentMousePos = pos;
		}

		void update_mouse_buttons(bool mouse1, bool mouse2, bool mouse3) {
			g_WorkingMouse.buttons[(int)MouseButton::MOUSE1] = mouse1;
			g_WorkingMouse.buttons[(int)MouseButton::MOUSE2] = mouse2;
			g_WorkingMouse.buttons[(int)MouseButton::MOUSE3] = mouse3;
		}
	}

	//void gamepad_callback(GameInputCallbackToken callback_token, void* context, IGameInputDevice* device, uint64_t timestamp, GameInputDeviceStatus current_status, GameInputDeviceStatus previous_status) {
	//	bool is_gamepad_1 = device == g_GamePad;

	//	if (current_status & GameInputDeviceConnected) {
	//		if ((!g_GamePad) && (!is_gamepad_1)) {

	//			if (!g_GamePad) {
	//				g_GamePad = device;
	//			}
	//		}

	//	}
	//	else {
	//		int i = 0;
	//		//log_l("disconnected\n");

	//		//if (is_gamepad_1) {
	//		//	log_l("gamepad 1\n");
	//		//	g_gamepad_1 = 0;
	//		//}
	//		//else if (is_gamepad_2) {
	//		//	log_l("gamepad 2\n");
	//		//	g_gamepad_2 = 0;
	//		//}
	//	}
	//}

	namespace Input {

		void initialize() {
			//HRESULT res = GameInputCreate(&g_GameInput);
			//assert(SUCCEEDED(res));

			//res = g_GameInput->RegisterDeviceCallback(
			//	nullptr,
			//	GameInputKindGamepad,
			//	GameInputDeviceConnected,
			//	GameInputBlockingEnumeration,
			//	nullptr,
			//	gamepad_callback,
			//	&g_CallbackToken
			//);

			//assert(SUCCEEDED(res));
		}

		void update() {
			//// ---------------------------- Gamepad ----------------------------
			//if (g_GamePad) {
			//	IGameInputReading* reading;
			//	HRESULT res = g_GameInput->GetCurrentReading(
			//		GameInputKindGamepad,
			//		g_GamePad,
			//		&reading
			//	);

			//	if (SUCCEEDED(res)) {
			//		// Gamepad buttons
			//		GameInputGamepadState state;
			//		reading->GetGamepadState(&state);
			//		reading->Release();
			//		reading = nullptr;

			//		if (state.buttons == GameInputGamepadNone) {
			//			g_CurrentGamepad.isAnyButtonDown = false;
			//		}
			//		else {
			//			g_CurrentGamepad.isAnyButtonDown = true;
			//		}

			//		for (int i = 0; i < static_cast<int>(GamepadButton::COUNT); ++i) {
			//			const uint32_t targetButton = state.buttons & (1 << i);
			//			g_CurrentGamepad.buttons[i] = targetButton != 0;
			//		}

			//		// Gamepad joysticks
			//		g_CurrentGamepad.leftTrigger = state.leftTrigger;
			//		g_CurrentGamepad.rightTrigger = state.rightTrigger;
			//		g_CurrentGamepad.leftThumbstick.x = state.leftThumbstickX;
			//		g_CurrentGamepad.leftThumbstick.y = state.leftThumbstickY;
			//		g_CurrentGamepad.rightThumbstick.x = state.rightThumbstickX;
			//		g_CurrentGamepad.rightThumbstick.y = state.rightThumbstickY;

			//		apply_deadzone(g_CurrentGamepad.leftThumbstick, 0.2f);
			//		apply_deadzone(g_CurrentGamepad.rightThumbstick, 0.2f);
			//	}
			//}

			// ---------------------- Keyboard and Mouse -----------------------
			g_CurrentKeyboard = g_WorkingKeyboard;
			g_CurrentMouse = g_WorkingMouse;

			if (g_CurrentMousePos.has_value()) {
				g_CurrentMouse.position = g_CurrentMousePos.value();
			}

			if (!g_LastMousePos.has_value()) {
				g_CurrentMouse.delta.x = 0;
				g_CurrentMouse.delta.y = 0;
			}
			else {
				g_CurrentMouse.delta.x = g_CurrentMousePos.value().x - g_LastMousePos.value().x;
				g_CurrentMouse.delta.y = g_CurrentMousePos.value().y - g_LastMousePos.value().y;
			}

			if (g_LastInputDevice != InputDevice::GAMEPAD && (g_CurrentGamepad.isAnyButtonDown ||
				(g_CurrentGamepad.leftThumbstick.x != 0.0f || g_CurrentGamepad.leftThumbstick.y != 0.0f ||
					g_CurrentGamepad.rightThumbstick.x != 0.0f || g_CurrentGamepad.rightThumbstick.y != 0.0f ||
					g_CurrentGamepad.leftTrigger != 0.0f || g_CurrentGamepad.rightTrigger != 0.0f))) {

				if ((g_CurrentMouse.delta.x == 0 && g_CurrentMouse.delta.y == 0) && !is_any_key_down()) {
					g_LastInputDevice = InputDevice::GAMEPAD;
				}
			}
			else if (g_LastInputDevice != InputDevice::KEYBOARD_AND_MOUSE) {
				if (!g_CurrentGamepad.isAnyButtonDown &&
					(g_CurrentGamepad.leftThumbstick.x == 0.0f && g_CurrentGamepad.leftThumbstick.y == 0.0f &&
						g_CurrentGamepad.rightThumbstick.x == 0.0f && g_CurrentGamepad.rightThumbstick.y == 0.0f &&
						g_CurrentGamepad.leftTrigger == 0.0f && g_CurrentGamepad.rightTrigger == 0.0f) &&
					(is_any_key_down() || (g_CurrentMouse.delta.x != 0 || g_CurrentMouse.delta.y != 0))) {

					g_LastInputDevice = InputDevice::KEYBOARD_AND_MOUSE;
				}
			}

			g_LastMousePos = g_CurrentMousePos;
		}

		void shutdown() {
			//g_GameInput->UnregisterCallback(g_CallbackToken, 5000);
			//g_GameInput->Release();
		}

		bool is_down(Key key) {
			return g_CurrentKeyboard.buttons[(int)key];
		}

		bool is_down(MouseButton button) {
			assert(button != MouseButton::BUTTON_COUNT);
			return g_CurrentMouse.buttons[(int)button];
		}

		bool is_down(GamepadButton button) {
			assert(button != GamepadButton::COUNT);
			return g_CurrentGamepad.buttons[(int)button];
		}

		bool is_any_key_down() {
			for (int i = (int)Key::SPACE; i <= (int)Key::LAST; ++i) {
				if (g_CurrentKeyboard.buttons[i]) {
					return true;
				}
			}

			return false;
		}

		bool is_gamepad_active() {
			return g_LastInputDevice == InputDevice::GAMEPAD;
		}

		bool is_gamepad_axes_active() {
			return (
				g_CurrentGamepad.leftThumbstick.x == 0.0f &&
				g_CurrentGamepad.leftThumbstick.y == 0.0f &&
				g_CurrentGamepad.rightThumbstick.x == 0.0f &&
				g_CurrentGamepad.rightThumbstick.y == 0.0f &&
				g_CurrentGamepad.leftTrigger == 0.0f &&
				g_CurrentGamepad.rightTrigger == 0.0f
				);
		}

		const GamepadState& get_gamepad_state() {
			return g_CurrentGamepad;
		}

		glm::ivec2 get_mouse_position() {
			return g_CurrentMouse.position;
		}

		glm::ivec2 get_mouse_delta() {
			return g_CurrentMouse.delta;
		}

		void set_thumbstick_deadzone(float deadzone) {
			//g_ThumbstickDeadzone = deadzone;
		}

	}
}