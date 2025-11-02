#pragma once

// TODO: Make platform agnostic
#include <Windows.h>
#include <string>
#include <vector>

struct SRWideTemp {
	std::vector<wchar_t> buffer;

	SRWideTemp(const char* utf8) {
		const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, nullptr, 0);

		if (n <= 0) {
			buffer.resize(1);
			buffer[0] = L'\0';
			return;
		}
		buffer.resize(n);

		MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			utf8,
			-1,
			buffer.data(),
			n
		);
	}

	operator const wchar_t* () const { return buffer.data(); }
};

namespace SRTextUtilities {
	inline std::wstring to_wide_string(const char* utf8) {
		if (!utf8) {
			return L"";
		}

		const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, nullptr, 0);

		if (n <= 0) {
			return L"";
		}

		std::wstring wide;
		wide.resize(n - 1);

		MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			utf8,
			-1,
			wide.data(),
			n
		);

		return wide;
	}

	inline std::wstring to_wide_string(const std::string& utf8) {
		return to_wide_string(utf8.c_str());
	}

	inline std::string to_string(const wchar_t* wide) {
		if (!wide) {
			return "";
		}

		const int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, nullptr, 0, nullptr, nullptr);

		if (n <= 0) {
			return "";
		}

		std::string utf8;
		utf8.resize(n - 1);
		WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			wide,
			-1,
			utf8.data(),
			n,
			nullptr,
			nullptr
		);

		return utf8;
	}

	inline std::string to_string(const std::wstring& wide) {
		return to_string(wide.c_str());
	}
}