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
