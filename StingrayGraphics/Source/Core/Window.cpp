#include "Window.h"
#include "Utilities/TextUtilities.h"

#include <imgui_impl_win32.h>

#include <vector>
#include <Windows.h>
#include <dwmapi.h>

#include <stdlib.h>

// TODO: Rename this to Window_Win32 and introduce other OS variants
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct SRWindow {
	HWND handle;
	u32 client_width;
	u32 client_height;

	SRWindow_OnResizeCallback on_resize_callback;
};

global bool g_is_window_class_registered = false;

// -------------------------- Impl Method Definitions --------------------------
internal LRESULT SRWindowWin32_WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
		return true;
	}

	// TRICK: WM_NCCREATE is guaranteed to be the first message that has the
	// valid HWND. Meaning that we can store the SRWindow pointer inside the
	// GWLP_USERDATA to then be used by other messages and also set m_Hwnd
	// as early as possible.
	if (msg == WM_NCCREATE) {
		CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
		SRWindow* window = (SRWindow*)cs->lpCreateParams;

		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window);
		window->handle = hwnd;
	}

	SRWindow* window = (SRWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (msg) {
	case WM_ERASEBKGND:
		return TRUE;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		{
			// TODO: Handle the edge case of window being minimized
			window->client_width = LOWORD(lParam);
			window->client_height = HIWORD(lParam);

			if (window->on_resize_callback) {
				window->on_resize_callback(window, window->client_width, window->client_height);
			}
		}
		break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

internal void SRWindowWin32_RegisterClassOnce() {
	if (g_is_window_class_registered) {
		return;
	}

	HINSTANCE instance = (HINSTANCE)GetModuleHandle(nullptr);
	HICON icon = LoadIcon(instance, MAKEINTRESOURCE(101));

	WNDCLASSEX window_class = {
		.cbSize = sizeof(WNDCLASSEX),
		.style = CS_OWNDC,
		.lpfnWndProc = SRWindowWin32_WindowProc,
		.hInstance = instance,
		.hIcon = icon,
		.hCursor = LoadCursor(nullptr, IDC_ARROW),
		.hbrBackground = nullptr,
		.lpszClassName = L"SRWindowWin32Class",
		.hIconSm = icon
	};
	RegisterClassEx(&window_class);

	g_is_window_class_registered = true;
}

internal void SRWindowWin32_Initialize(const char* name, u32 width, u32 height, SRWindowFlags flags, SRWindow* window) {
	SRWideTemp wide_name = SRWideTemp(name);
	HINSTANCE instance = (HINSTANCE)GetModuleHandle(nullptr);

	// TODO: We might eventually want multiple windows, and as such it
	// does not make sense to create a new window class for each window.
	// Instead we should move this logic elsewhere.
	

	BOOL has_menu = FALSE;
	int window_styles = WS_OVERLAPPEDWINDOW;
	int window_width = width;
	int window_height = height;
	int window_pos_x = CW_USEDEFAULT;
	int window_pos_y = CW_USEDEFAULT;

	if (flags & SRWindowFlags_SizeIsClientArea) {
		RECT windowRect = { 0, 0, (LONG)width, (LONG)height };
		AdjustWindowRect(&windowRect, window_styles, has_menu);
		window_width = (int)(windowRect.right - windowRect.left);
		window_height = (int)(windowRect.bottom - windowRect.top);
	}

	if (flags & SRWindowFlags_Centered) {
		HMONITOR primary_monitor = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);

		MONITORINFO monitor_info = { .cbSize = sizeof(MONITORINFO) };
		GetMonitorInfo(primary_monitor, &monitor_info);

		int monitor_width = (int)(labs(monitor_info.rcMonitor.right - monitor_info.rcMonitor.left));
		int monitor_height = (int)(labs(monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top));
		int monitor_pos_x = (int)monitor_info.rcMonitor.left;
		int monitor_pos_y = (int)monitor_info.rcMonitor.top;

		window_pos_x = monitor_pos_x + (monitor_width - window_width) / 2;
		window_pos_y = monitor_pos_y + (monitor_height - window_height) / 2;
	}

	window->handle = CreateWindowEx(
		WS_EX_APPWINDOW,
		L"SRWindowWin32Class",
		wide_name,
		window_styles,
		window_pos_x,
		window_pos_y,
		window_width,
		window_height,
		nullptr,
		nullptr,
		instance,
		window
	);
	RECT client_rect;
	GetClientRect(window->handle, &client_rect);
	window->client_width = (u32)(client_rect.right - client_rect.left);
	window->client_height = (u32)(client_rect.bottom - client_rect.top);

	BOOL use_dark_mode = TRUE;
	DwmSetWindowAttribute(window->handle, DWMWA_USE_IMMERSIVE_DARK_MODE, &use_dark_mode, sizeof(use_dark_mode));
}

SRWindow* SRWindow_Create(const char* name, u32 width, u32 height, SRWindowFlags flags) {
	SRWindow* window = (SRWindow*)malloc(sizeof(SRWindow));
	assert(window);
	ZeroMemory(window, sizeof(*window));
	
	SRWindowWin32_RegisterClassOnce();
	SRWindowWin32_Initialize(name, width, height, flags, window);

	return window;
}

void SRWindow_Destroy(SRWindow* window) {
	free(window);
}

bool SRWindow_PollEvents(SRWindow* window) {
	MSG msg = {};

	while (PeekMessage(&msg, nullptr, 0, WM_INPUT - 1, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			return false;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	while (PeekMessage(&msg, nullptr, WM_INPUT + 1, (UINT)-1, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return true;
}

void SRWindow_Show(SRWindow* window) {
	if (!IsWindowVisible(window->handle)) {
		ShowWindow(window->handle, SW_SHOWNORMAL);
		return;
	}
}

void* SRWindow_GetInternalHandle(SRWindow* window) {
	return window->handle;
}

void SRWindow_GetClientSize(SRWindow* window, u32* width, u32* height) {
	*width = window->client_width;
	*height = window->client_height;
}

f32 SRWindow_GetClientAspectRatio(SRWindow* window) {
	return (f32)window->client_width / (f32)window->client_height;
}

void SRWindow_SetOnResizeCallback(SRWindow* window, SRWindow_OnResizeCallback callback) {
	window->on_resize_callback = callback;
}
