#include "StringTypes.h"

#include <Windows.h>
#include <assert.h>

void Str8_ToStr16(const Str8* str8, Str16* str16) {
	//int str8_len = MultiByte

	//int n = MultiByteToWideChar(
	//	CP_UTF8,
	//	MB_ERR_INVALID_CHARS,
	//	(char*)str8->data,
	//	(int)str8->size,
	//	(wchar_t*)str16->data,
	//	(int)str16->len
	//);
}
void Str16_ToStr8(const Str16* str16, Str8* str8) {
	int req_str8_size = WideCharToMultiByte(
		CP_UTF8,
		WC_ERR_INVALID_CHARS,
		(wchar_t*)str16->data,
		-1,
		nullptr,
		0,
		nullptr,
		nullptr
	);
	assert(str8->size >= req_str8_size);

	int num_bytes_written = WideCharToMultiByte(
		CP_UTF8,
		WC_ERR_INVALID_CHARS,
		(wchar_t*)str16->data,
		-1,
		(char*)str8->data,
		req_str8_size,
		nullptr,
		nullptr
	);
	assert(num_bytes_written != 0);
}

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
	return {};
}
