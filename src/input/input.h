#pragma once

namespace input {
	struct KeyboardState {
		bool buttons[512] = { false };
		bool downOnceButtons[512] = { false };
	};

	struct MouseState {
		float deltaX = 0.0f;
		float deltaY = 0.0f;
		bool buttons[8] = { false };
	};

	void initialize(); // NOTE: Required to be called on startup
	// TODO: Refactor this
	void process_mouse_position(double x, double y);
	void process_mouse_event(int button, int action, int mods);
	void process_key_event(int key, int scancode, int action, int mods);
	void get_keyboard_state(KeyboardState& keyboard);
	void get_mouse_state(MouseState& mouse);
	void update(); // NOTE: Should ideally be called once every frame

	bool is_key_down(int keyCode);
	bool is_key_down_once(int keyCode);
	bool is_mouse_down(int button);
}