#include "input.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <optional>

namespace input {
	static KeyboardState g_CurrentKeyboard = {}; // NOTE: Only updated when update() is called
	static MouseState g_CurrentMouse = {}; // NOTE: Onlyupdated when update() is called

	static KeyboardState g_WorkingKeyboard = {}; // NOTE: Updated when parseKeyEvent() functions are called
	static MouseState g_WorkingMouse = {}; // NOTE: Updated when parseMouseEvent() is called

	static std::optional<glm::vec2> g_LastMousePos = std::nullopt;
	static std::optional<glm::vec2> g_CurrentMousePos = std::nullopt;

	void initialize() {

	}

	void process_mouse_position(double x, double y) {
		g_CurrentMousePos = { x, y };

		if (!g_LastMousePos.has_value()) {
			g_LastMousePos = g_CurrentMousePos;
		}
	}

	void process_mouse_event(int button, int action, int mods) {
		g_WorkingMouse.buttons[button] = action != GLFW_RELEASE;
	}

	void process_key_event(int key, int scancode, int action, int mods) {
		g_WorkingKeyboard.buttons[key] = action != GLFW_RELEASE;
	}

	void get_keyboard_state(KeyboardState& keyboard) {
		keyboard = g_CurrentKeyboard;
	}

	void get_mouse_state(MouseState& mouse) {
		mouse = g_CurrentMouse;
	}

	void update() {
		g_CurrentKeyboard = g_WorkingKeyboard;
		g_CurrentMouse = g_WorkingMouse;

		if (!g_LastMousePos.has_value() || !g_CurrentMousePos.has_value()) {
			g_CurrentMouse.deltaX = 0;
			g_CurrentMouse.deltaY = 0;
			return;
		}

		g_CurrentMouse.deltaX = g_CurrentMousePos.value().x - g_LastMousePos.value().x;
		g_CurrentMouse.deltaY = g_CurrentMousePos.value().y - g_LastMousePos.value().y;

		g_LastMousePos = g_CurrentMousePos;
	}

	bool is_key_down(int keyCode) {
		return g_CurrentKeyboard.buttons[(size_t)keyCode];
	}

	bool is_mouse_down(int button) {
		return g_CurrentMouse.buttons[(size_t)button];
	}

}