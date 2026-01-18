#include "StringTypes.h"

#include <Windows.h>
#include <assert.h>

Str8 Str8_Build(u8* data, u64 size) {
	return Str8{ data, size };
}

Str8 Str8_Concat(SRArena* arena, Str8 str1, Str8 str2) {
	assert(str1.data || str1.size == 0);
	assert(str2.data || str2.size == 0);

	u64 concat_size = str1.size + str2.size;
	u8* concat_data = SRArena_PushArray(arena, u8, concat_size + 1);

	memcpy(concat_data, str1.data, str1.size);
	memcpy(concat_data + str1.size, str2.data, str2.size);
	concat_data[concat_size] = '\0';

	return { concat_data, concat_size };
}

u64 Str8_FindFirstOf(Str8 str, u8 c) {
	for (u64 i = 0; i < str.size; ++i) {
		if (str.data[i] == c) {
			return i;
		}
	}

	return STR8_NOT_FOUND;
}

u64 Str8_FindLastOf(Str8 str, u8 c) {
	if (str.size == 0) {
		return STR8_NOT_FOUND;
	}

	u64 i = str.size;
	do {
		--i;
		if (str.data[i] == c) {
			return i;
		}
	} while (i != 0);

	return STR8_NOT_FOUND;
}

Str8 Str8_FromStr16(SRArena* arena, Str16 str16) {
	assert(str16.data);

	if (str16.size == 0) {
		return Str8{};
	}

	int wchar_count = (int)(str16.size / sizeof(WCHAR));
	int str8_size = (u64)WideCharToMultiByte(
		CP_UTF8,
		WC_ERR_INVALID_CHARS,
		(WCHAR*)str16.data,
		wchar_count,
		nullptr,
		0,
		nullptr,
		nullptr
	);
	assert(str8_size > 0);

	Str8 str8 = { SRArena_PushArray(arena, u8, str8_size + 1), (u64)str8_size };
	int num_bytes_written = WideCharToMultiByte(
		CP_UTF8,
		WC_ERR_INVALID_CHARS,
		(WCHAR*)str16.data,
		wchar_count,
		(char*)str8.data,
		str8_size,
		nullptr,
		nullptr
	);
	assert(num_bytes_written == str8_size);

	str8.data[str8.size] = '\0';

	return str8;
}

Str16 Str16_Build(u16* data, u64 size) {
	return Str16{ data, size };
}

Str16 Str16_FromStr8(SRArena* arena, Str8 str8) {
	assert(str8.data);
	assert(sizeof(u16) == sizeof(WCHAR)); // NOTE: Will in practice always be true

	if (str8.size == 0) {
		return Str16{};
	}

	int num_wchars = MultiByteToWideChar(
		CP_UTF8,
		MB_ERR_INVALID_CHARS,
		(char*)str8.data,
		(int)str8.size,
		nullptr,
		0
	);

	Str16 str16 = { SRArena_PushArray(arena, u16, num_wchars + 1), num_wchars * sizeof(u16) };
	int num_wchars_written = MultiByteToWideChar(
		CP_UTF8,
		MB_ERR_INVALID_CHARS,
		(char*)str8.data,
		(int)str8.size,
		(WCHAR*)str16.data,
		num_wchars
	);
	assert(num_wchars_written * sizeof(u16) == str16.size);
	
	str16.data[num_wchars_written] = L'\0';

	return str16;
}
