#include "Window.h"
#include "Platform.h"
#include "Input/Input.h"

#include <Windows.h>
#include <windowsx.h>
#include "Resource.h"

#include <chrono>
#include <format>
#include <iostream>

namespace SR {
	// Helpers
	INTERNAL inline std::wstring to_wide_string(const std::string& str) {
		std::wstring wstr;
		size_t size;
		wstr.resize(str.length());
		mbstowcs_s(&size, &wstr[0], wstr.size() + 1, str.c_str(), str.size());

		return wstr;
	}

	INTERNAL constexpr Key convert_key(WPARAM wParam, LPARAM lParam) {
		switch (wParam) {
		case VK_CONTROL:
			return (lParam & 0x01000000) ? Key::RIGHT_CONTROL : Key::LEFT_CONTROL;
		case VK_SHIFT:
		{
			const UINT scancode = (lParam & 0x00ff0000) >> 16;
			return (MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX) == VK_RSHIFT)
				? Key::RIGHT_SHIFT
				: Key::LEFT_SHIFT;
		}
		case VK_MENU: return (lParam & 0x01000000) ? Key::RIGHT_ALT : Key::LEFT_ALT;
		case VK_SPACE: return Key::SPACE;
		case VK_ESCAPE: return Key::ESCAPE;
		case VK_RETURN: return Key::ENTER;
		case VK_TAB: return Key::TAB;
		case VK_BACK: return Key::BACKSPACE;
		case VK_INSERT: return Key::INSERT;
		case VK_DELETE: return Key::DEL;
		case VK_LEFT: return Key::LEFT;
		case VK_RIGHT: return Key::RIGHT;
		case VK_UP: return Key::UP;
		case VK_DOWN: return Key::DOWN;
		case VK_PRIOR: return Key::PAGE_UP;
		case VK_NEXT: return Key::PAGE_DOWN;
		case VK_HOME: return Key::HOME;
		case VK_END: return Key::END;
		case VK_CAPITAL: return Key::CAPS_LOCK;
		case VK_SCROLL: return Key::SCROLL_LOCK;
		case VK_NUMLOCK: return Key::NUM_LOCK;
		case VK_SNAPSHOT: return Key::PRINT_SCREEN;
		case VK_PAUSE: return Key::PAUSE;

			// Function keys
		case VK_F1: return Key::F1;
		case VK_F2: return Key::F2;
		case VK_F3: return Key::F3;
		case VK_F4: return Key::F4;
		case VK_F5: return Key::F5;
		case VK_F6: return Key::F6;
		case VK_F7: return Key::F7;
		case VK_F8: return Key::F8;
		case VK_F9: return Key::F9;
		case VK_F10: return Key::F10;
		case VK_F11: return Key::F11;
		case VK_F12: return Key::F12;

			// Numeric keypad
		case VK_NUMPAD0: return Key::KP_0;
		case VK_NUMPAD1: return Key::KP_1;
		case VK_NUMPAD2: return Key::KP_2;
		case VK_NUMPAD3: return Key::KP_3;
		case VK_NUMPAD4: return Key::KP_4;
		case VK_NUMPAD5: return Key::KP_5;
		case VK_NUMPAD6: return Key::KP_6;
		case VK_NUMPAD7: return Key::KP_7;
		case VK_NUMPAD8: return Key::KP_8;
		case VK_NUMPAD9: return Key::KP_9;
		case VK_DECIMAL: return Key::KP_DECIMAL;
		case VK_DIVIDE: return Key::KP_DIVIDE;
		case VK_MULTIPLY: return Key::KP_MULTIPLY;
		case VK_SUBTRACT: return Key::KP_SUBTRACT;
		case VK_ADD: return Key::KP_ADD;

			// Alphabetic keys
		case 'A': return Key::A;
		case 'B': return Key::B;
		case 'C': return Key::C;
		case 'D': return Key::D;
		case 'E': return Key::E;
		case 'F': return Key::F;
		case 'G': return Key::G;
		case 'H': return Key::H;
		case 'I': return Key::I;
		case 'J': return Key::J;
		case 'K': return Key::K;
		case 'L': return Key::L;
		case 'M': return Key::M;
		case 'N': return Key::N;
		case 'O': return Key::O;
		case 'P': return Key::P;
		case 'Q': return Key::Q;
		case 'R': return Key::R;
		case 'S': return Key::S;
		case 'T': return Key::T;
		case 'U': return Key::U;
		case 'V': return Key::V;
		case 'W': return Key::W;
		case 'X': return Key::X;
		case 'Y': return Key::Y;
		case 'Z': return Key::Z;

			// Digits
		case '0': return Key::ALPHA0;
		case '1': return Key::ALPHA1;
		case '2': return Key::ALPHA2;
		case '3': return Key::ALPHA3;
		case '4': return Key::ALPHA4;
		case '5': return Key::ALPHA5;
		case '6': return Key::ALPHA6;
		case '7': return Key::ALPHA7;
		case '8': return Key::ALPHA8;
		case '9': return Key::ALPHA9;

			// Special keys
		case VK_OEM_1: return Key::SEMICOLON;
		case VK_OEM_PLUS: return Key::EQUAL;
		case VK_OEM_COMMA: return Key::COMMA;
		case VK_OEM_MINUS: return Key::MINUS;
		case VK_OEM_PERIOD: return Key::PERIOD;
		case VK_OEM_2: return Key::SLASH;
		case VK_OEM_3: return Key::GRAVE_ACCENT;
		case VK_OEM_4: return Key::LEFT_BRACKET;
		case VK_OEM_5: return Key::BACKSLASH;
		case VK_OEM_6: return Key::RIGHT_BRACKET;
		case VK_OEM_7: return Key::APOSTROPHE;

		default: return Key::LAST; // Fallback to a "null" value
		}
	}

	// ------------------------- Window Implementation -------------------------
	struct Window::Impl {
		static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

		std::string m_Name;
		int m_ClientWidth;
		int m_ClientHeight;
		WindowFlag m_Flags;
		bool m_ShouldClose = false;
		HWND m_Handle;

		window_resize_callback m_ResizeCallback = nullptr;
		window_mouse_pos_callback m_MousePosCallback = nullptr;
		window_keyboard_callback m_KeyboardCallback = nullptr;
	};

	LRESULT CALLBACK Window::Impl::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
		Window::Impl* pWindow = reinterpret_cast<Window::Impl*>(
			GetWindowLongPtr(window, GWLP_USERDATA)
		);

		switch (message) {
		case WM_NCCREATE:
		{
			CREATESTRUCT* pCS = reinterpret_cast<CREATESTRUCT*>(lParam);
			pWindow = reinterpret_cast<Window::Impl*>(pCS->lpCreateParams);
			SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)pWindow);
		}
		break;
		case WM_NCCALCSIZE:
			{
				if (pWindow != nullptr && !has_flag(pWindow->m_Flags, WindowFlag::NO_TITLEBAR)) {
					break;
				}

				const UINT dpi = GetDpiForWindow(window);
				const int frameX = GetSystemMetricsForDpi(SM_CXFRAME, dpi);
				const int frameY = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
				const int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

				NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lParam;
				RECT* requestedClientRect = params->rgrc;

				bool isMaximized = false;
				WINDOWPLACEMENT placement = { 0 };
				placement.length = sizeof(WINDOWPLACEMENT);
				if (GetWindowPlacement(window, &placement)) {
					isMaximized = placement.showCmd == SW_SHOWMAXIMIZED;
				}

				if (isMaximized) {
					const int sizeFrameY = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi);
					requestedClientRect->right -= frameY + padding;
					requestedClientRect->left += frameX + padding;
					requestedClientRect->top += sizeFrameY + padding;
					requestedClientRect->bottom -= frameY + padding;
				}
				else {
					// ------ Hack to remove the title bar (non-client) area ------
					// In order to hide the standard title bar we must change
					// the NCCALCSIZE_PARAMS, which dictates the title bar area.
					//
					// In Windows 10 we can set the top component to '0', which
					// in effect hides the default title bar.
					// However, for unknown reasons this does not work in
					// Windows 11, instead we are required to set the top
					// component to '1' in order to get the same effect.
					//
					// Thus, we must first check the Windows version before
					// altering the NCCALCSIZE_PARAMS structure.
					const int cxBorder = 1;
					const int cyBorder = 1;
					InflateRect((LPRECT)lParam, -cxBorder, -cyBorder);
				}

				return 0;
			}
			break;
		case WM_NCHITTEST:
			{
				if (!has_flag(pWindow->m_Flags, WindowFlag::NO_TITLEBAR)) {
					break;
				}

				const POINT ptMouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

				RECT rcWindow;
				GetWindowRect(window, &rcWindow);
				// NOTE: GetWindowRect does not really give us the coords for
				// bottom-right corner, in fact the bottom-right corner is
				// calculated as (rcWindow.right - 1, rcWindow.bottom - 1).
				// For convenience in the checks below, we update the rcWindow
				// bottom-right members for this reason.
				rcWindow.right -= 1;
				rcWindow.bottom -= 1;

				const bool insideWindow = (
					ptMouse.x >= rcWindow.left && ptMouse.x <= rcWindow.right &&
					ptMouse.y >= rcWindow.top && ptMouse.y <= rcWindow.bottom
				);

				const int sizingBorder = 8; // TODO: Perhaps make dynamic
				const int titlebarHeight = 31; // TODO: Perhaps make dynamic
				const int sysButtonWidth = 44; // TODO: Will break for dynamic DPI
				const bool top = ptMouse.y >= rcWindow.top && ptMouse.y < rcWindow.top + sizingBorder;
				const bool left = ptMouse.x >= rcWindow.left && ptMouse.x < rcWindow.left + sizingBorder;
				const bool bottom = ptMouse.y <= rcWindow.bottom && ptMouse.y > rcWindow.bottom - sizingBorder;
				const bool right = ptMouse.x <= rcWindow.right && ptMouse.x > rcWindow.right - sizingBorder;
				const bool caption = ptMouse.y <= rcWindow.top + titlebarHeight;

				if (caption) {
					// Guaranteed to be in the sys-button area
					if (ptMouse.x >= rcWindow.right - sysButtonWidth * 3 &&
						ptMouse.x < rcWindow.right &&
						ptMouse.y > rcWindow.top
					) {
						if (ptMouse.x >= rcWindow.right - sysButtonWidth) {
							return HTCLOSE;
						}
						if (ptMouse.x >= rcWindow.right - sysButtonWidth * 2) {
							return HTMAXBUTTON;
						}

						return HTMINBUTTON;
					}

					// If this is reached, we know that we are in the caption
					// area, but not inside the sys-button area. I.e. we can
					// ignore the check for bottom, bottom-left and bottom-right
					if (top && left) { return HTTOPLEFT; }
					if (top && right) { return HTTOPRIGHT; }
					if (top) { return HTTOP; }
					if (left) { return HTLEFT; }
					if (right) { return HTRIGHT; }

					return HTCAPTION;
				}

				// If this is reached, we can ignore the check for top, top-left
				// and top-right. That is already handled above.
				if (bottom && left) { return HTBOTTOMLEFT; }
				if (bottom && right) { return HTBOTTOMRIGHT; }
				if (left) { return HTLEFT; }
				if (right) { return HTRIGHT; }
				if (bottom) { return HTBOTTOM; }

				// If this is reached, we know that the cursor is not on the
				// border, and must therefore be in the client area.
				return HTCLIENT;
			}
			break;
		case WM_ERASEBKGND:
			return TRUE;
		case WM_SIZE:
		{
			if (pWindow) {
				const int width = LOWORD(lParam);
				const int height = HIWORD(lParam);

				pWindow->m_ClientWidth = width;
				pWindow->m_ClientHeight = height;

				if (pWindow->m_ResizeCallback) {
					pWindow->m_ResizeCallback(width, height);
				}
			}
		}
		break;
		case WM_MOUSEMOVE:
		{
			TRACKMOUSEEVENT tme{};
			tme.cbSize = sizeof(tme);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = window;

			TrackMouseEvent(&tme);
			POINT pos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

			Input::Internal::update_mouse_position({ (int)pos.x, (int)pos.y });

			if (pWindow && pWindow->m_MousePosCallback) {
				pWindow->m_MousePosCallback((int)pos.x, (int)pos.y);
			}
		}
		break;
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_XBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
		case WM_XBUTTONUP:
		{
			Input::Internal::update_mouse_buttons(
				(GET_KEYSTATE_WPARAM(wParam) & MK_LBUTTON) > 0,
				(GET_KEYSTATE_WPARAM(wParam) & MK_RBUTTON) > 0,
				(GET_KEYSTATE_WPARAM(wParam) & MK_MBUTTON) > 0
			);
		}
		break;
		case WM_MOUSELEAVE:
		{
			Input::Internal::update_mouse_buttons(false, false, false);
		}
		break;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYUP:
		{
			const Key key = convert_key(wParam, lParam);
			Input::Internal::update_key_state(key, message == WM_KEYDOWN ? true : false);

			if (pWindow && pWindow->m_KeyboardCallback) {
				pWindow->m_KeyboardCallback(key, ButtonAction::NONE, ButtonMods::NONE);
			}
		}
		break;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}

		return DefWindowProc(window, message, wParam, lParam);
	}

	// --------------------------- Window Interface ----------------------------
	Window::Window(const std::string& name, int width, int height, WindowFlag flags) {
		m_Impl = new Impl();
		m_Impl->m_Name = name;
		m_Impl->m_ClientWidth = width;
		m_Impl->m_ClientHeight = height;
		m_Impl->m_Flags = flags;

		const std::wstring wName = to_wide_string(m_Impl->m_Name);
		const HINSTANCE hInstance = GetModuleHandle(nullptr);

		const WNDCLASSEX windowClass = {
			.cbSize = sizeof(WNDCLASSEX),
			.style = CS_OWNDC,
			.lpfnWndProc = m_Impl->WindowProc,
			.hInstance = hInstance,
			.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON)),
			.hCursor = LoadCursor(nullptr, IDC_ARROW),
			.hbrBackground = nullptr,
			.lpszMenuName = nullptr,
			.lpszClassName = wName.c_str(),
			.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON))
		};
		RegisterClassEx(&windowClass);

		RECT windowRect = { 0, 0, m_Impl->m_ClientWidth, m_Impl->m_ClientHeight };

		// Use the width and height as the dimensions for the client area
		if (has_flag(m_Impl->m_Flags, WindowFlag::SIZE_IS_CLIENT_AREA)) {
			if (has_flag(m_Impl->m_Flags, WindowFlag::NO_TITLEBAR)) {
				const int borderX = GetSystemMetrics(SM_CXBORDER);
				const int borderY = GetSystemMetrics(SM_CYBORDER);

				windowRect.right += borderX * 2;
				windowRect.bottom += borderY * 2;
			}
			else {
				AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
			}
		}

		const int windowWidth = windowRect.right - windowRect.left;
		const int windowHeight = windowRect.bottom - windowRect.top;

		// Window centering
		POINT windowPosition = { CW_USEDEFAULT, CW_USEDEFAULT };

		if (has_flag(m_Impl->m_Flags, WindowFlag::CENTER)) {
			const HMONITOR primaryMonitor = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
			MONITORINFO monitorInfo = { .cbSize = sizeof(monitorInfo) };

			GetMonitorInfo(primaryMonitor, &monitorInfo);
			const int monitorWidth = std::abs(monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left);
			const int monitorHeight = std::abs(monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top);

			windowPosition.x = monitorInfo.rcMonitor.left + monitorWidth / 2 - windowWidth / 2;
			windowPosition.y = monitorInfo.rcMonitor.top + monitorHeight / 2 - windowHeight / 2;
		}

		// NOTE: For windows without a native title bar, there's a couple of
		// tricks that need to be employed. First of all, WS_SYSMENU style must
		// be removed, this is because this style influences how mouse position
		// is reported in some Windows messages. For example, the lParam in
		// WM_NCHITTEST gives us the relative mouse position. However, when
		// WS_SYSMENU is a present window-style, the reported mouse position
		// will be incorrect in the sys-button area.
		//
		// More specifically, when the mouse is in the sys-button area, we will
		// never get mouse.y == windowRect.top or mouse.x == windowRect.right as
		// a reported mouse position. This makes it impossible to properly check
		// if the mouse is positioned on the border in this area.
		// This is of course only the case for non-titlebar windows, and this
		// issue is a direct result of how we handle WM_NCCALCSIZE.
		// See WM_NCCALCSIZE code for further details.
		// Thus, in order to get correct mouse-position reporting, we simply
		// disable the WS_SYSMENU windows style.
		m_Impl->m_Handle = CreateWindowEx(
			WS_EX_APPWINDOW,
			windowClass.lpszClassName,
			windowClass.lpszClassName,
			WS_OVERLAPPEDWINDOW & (~WS_SYSMENU),
			static_cast<int>(windowPosition.x),
			static_cast<int>(windowPosition.y),
			windowWidth,
			windowHeight,
			nullptr,
			nullptr,
			windowClass.hInstance,
			m_Impl
		);
	}

	Window::~Window() {
		delete m_Impl;
		m_Impl = nullptr;
	}

	void Window::poll_events() {
		MSG msg = {};

		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				m_Impl->m_ShouldClose = true;
			}
			else {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	void Window::show() {
		ShowWindow(m_Impl->m_Handle, SW_NORMAL);
	}

	bool Window::should_close() {
		return m_Impl->m_ShouldClose;
	}

	int Window::get_client_width() const {
		return m_Impl->m_ClientWidth;
	}

	int Window::get_client_height() const {
		return m_Impl->m_ClientHeight;
	}

	float Window::get_client_aspect_ratio() const {
		return static_cast<float>(m_Impl->m_ClientWidth) / m_Impl->m_ClientHeight;
	}

	void* Window::get_internal_object() {
		return m_Impl->m_Handle;
	}

	void Window::set_resize_callback(window_resize_callback callback) {
		m_Impl->m_ResizeCallback = callback;
	}

	void Window::set_mouse_pos_callback(window_mouse_pos_callback callback) {
		m_Impl->m_MousePosCallback = callback;
	}

	void Window::set_keyboard_callback(window_keyboard_callback callback) {
		m_Impl->m_KeyboardCallback = callback;
	}

}