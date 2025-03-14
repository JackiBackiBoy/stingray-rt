#pragma once

#include "Core/EnumFlags.h"
#include "Input/Input.h"

#include <cstdint>
#include <string>

namespace SR {
	enum class WindowFlag : uint8_t {
		NONE = 0,
		CENTER = (1 << 0),
		SIZE_IS_CLIENT_AREA = (1 << 1),
		NO_TITLEBAR = (1 << 2),
	};

	template<>
	struct enable_bitmask_operators<WindowFlag> { static constexpr bool enable = true; };

	typedef void (*window_resize_callback)(int width, int height);
	typedef void (*window_mouse_pos_callback)(int x, int y);
	typedef void (*window_mouse_button_callback)(MouseButton button, ButtonAction action, ButtonMods mods);
	typedef void (*window_keyboard_callback)(Key key, ButtonAction action, ButtonMods mods);

	class Window {
	public:
		Window(const std::string& name, int width, int height, WindowFlag flags = WindowFlag::NONE);
		~Window();

		void poll_events();
		void show();
		bool should_close();

		int get_client_width() const;
		int get_client_height() const;
		float get_client_aspect_ratio() const;
		void* get_internal_object();
		void set_resize_callback(window_resize_callback callback);
		void set_mouse_pos_callback(window_mouse_pos_callback callback);
		void set_mouse_button_callback(window_mouse_button_callback callback);
		void set_keyboard_callback(window_keyboard_callback callback);

	private:
		struct Impl;
		Impl* m_Impl;
	};
}
