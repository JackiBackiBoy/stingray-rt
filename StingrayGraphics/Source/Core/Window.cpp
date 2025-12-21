#include "Window.h"
#include "Utilities/TextUtilities.h"

#include <imgui_impl_win32.h>

#include <vector>
#include <Windows.h>
#include <dwmapi.h>

// TODO: Rename this to Window_Win32 and introduce other OS variants
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ------------------------------ Impl Definition ------------------------------
struct SRWindow::Impl {
	HWND m_Hwnd = nullptr;
	bool m_QuitRequested = false;

	static LRESULT window_proc_thunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void create_window(const char* title, int width, int height, SRWindowFlags flags);
	LRESULT window_proc(UINT msg, WPARAM wParam, LPARAM lParam);
};

// -------------------------- Impl Method Definitions --------------------------
LRESULT SRWindow::Impl::window_proc_thunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
		return true;
	}
	// TODO: // (You should discard mouse/keyboard messages in your game/engine when io.WantCaptureMouse/io.WantCaptureKeyboard are set.)
	// From ImGui documentation

	// TRICK: WM_NCCREATE is guaranteed to be the first message that has the
	// valid HWND. Meaning that we can store the SRWindow pointer inside the
	// GWLP_USERDATA to then be used by other messages and also set m_Hwnd
	// as early as possible.
	if (msg == WM_NCCREATE) {
		auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		auto impl = static_cast<SRWindow::Impl*>(cs->lpCreateParams);

		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl));
		impl->m_Hwnd = hWnd;
	}

	auto impl = reinterpret_cast<SRWindow::Impl*>(
		GetWindowLongPtr(hWnd, GWLP_USERDATA)
	);

	if (impl) {
		return impl->window_proc(msg, wParam, lParam);
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void SRWindow::Impl::create_window(const char* title, int width, int height, SRWindowFlags flags) {
	const SRWideTemp wTitle = SRWideTemp(title);
	const HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(nullptr);

	// TODO: We might eventually want multiple windows, and as such it
	// does not make sense to create a new window class for each window.
	// Instead we should move this logic elsewhere.
	HICON icon = LoadIcon(hInstance, MAKEINTRESOURCE(101));
	const WNDCLASSEX wndClassEx = {
		.cbSize = sizeof(WNDCLASSEX),
		.style = CS_OWNDC,
		.lpfnWndProc = window_proc_thunk,
		.hInstance = hInstance,
		.hIcon = icon,
		.hCursor = LoadCursor(nullptr, IDC_ARROW),
		.hbrBackground = nullptr,
		.lpszClassName = L"SRWindowClass",
		.hIconSm = icon
	};
	RegisterClassEx(&wndClassEx);

	BOOL hasMenu = FALSE;
	int windowStyles = WS_OVERLAPPEDWINDOW;
	int windowWidth = width;
	int windowHeight = height;
	int windowPosX = CW_USEDEFAULT;
	int windowPosY = CW_USEDEFAULT;

	if (flags & SRWindowFlags_SizeIsClientArea) {
		RECT windowRect = { 0, 0, width, height };
		AdjustWindowRect(&windowRect, windowStyles, hasMenu);
		windowWidth = static_cast<int>(windowRect.right - windowRect.left);
		windowHeight = static_cast<int>(windowRect.bottom - windowRect.top);
	}

	if (flags & SRWindowFlags_Centered) {
		const HMONITOR primaryMonitor = MonitorFromPoint(
			{ 0, 0 },
			MONITOR_DEFAULTTOPRIMARY
		);

		MONITORINFO monitorInfo = { .cbSize = sizeof(MONITORINFO) };
		GetMonitorInfo(primaryMonitor, &monitorInfo);

		const RECT& monitorRect = monitorInfo.rcMonitor;
		const int monitorWidth = static_cast<int>(std::labs(monitorRect.right - monitorRect.left));
		const int monitorHeight = static_cast<int>(std::labs(monitorRect.bottom - monitorRect.top));
		const int monitorPosX = static_cast<int>(monitorRect.left);
		const int monitorPosY = static_cast<int>(monitorRect.top);

		windowPosX = monitorPosX + (monitorWidth - windowWidth) / 2;
		windowPosY = monitorPosY + (monitorHeight - windowHeight) / 2;
	}

	m_Hwnd = CreateWindowEx(
		0,
		wndClassEx.lpszClassName,
		wTitle,
		windowStyles,
		windowPosX,
		windowPosY,
		windowWidth,
		windowHeight,
		nullptr,
		nullptr,
		hInstance,
		this
	);

	const BOOL useDarkMode = TRUE;
	DwmSetWindowAttribute(m_Hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
}

LRESULT SRWindow::Impl::window_proc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CLOSE:
		DestroyWindow(m_Hwnd);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(m_Hwnd, msg, wParam, lParam);
	}
}

// --------------------------------- Public API --------------------------------
SRWindow::SRWindow(const char* name, int width, int height, SRWindowFlags flags) {
	m_Impl = new Impl();
	m_Impl->create_window(name, width, height, flags);
}

SRWindow::~SRWindow() {
	// TODO: Perhaps move the window class to be global for all windows, i.e.
	// we should only destroy this after every other window using it has been
	// destroyed.
	UnregisterClass(L"SRWindowClass", (HINSTANCE)GetModuleHandle(nullptr));

	if (m_Impl) {
		delete m_Impl;
		m_Impl = nullptr;
	}
}

bool SRWindow::poll_events() {
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

void SRWindow::show() {
	if (!IsWindowVisible(m_Impl->m_Hwnd)) {
		ShowWindow(m_Impl->m_Hwnd, SW_SHOWNORMAL);
		return;
	}

	// TODO: Handle "restore" of window
}

void* SRWindow::get_internal_handle() const {
	return m_Impl->m_Hwnd;
}

void SRWindow::get_client_size(int* width, int* height) const {
	RECT clientRect;
	GetClientRect(m_Impl->m_Hwnd, &clientRect);

	if (width) {
		*width = static_cast<int>(clientRect.right - clientRect.left);
	}

	if (height) {
		*height = static_cast<int>(clientRect.bottom - clientRect.top);
	}
}

float SRWindow::get_client_aspect_ratio() const {
	int width;
	int height;
	get_client_size(&width, &height);

	return static_cast<float>(width) / height;
}
