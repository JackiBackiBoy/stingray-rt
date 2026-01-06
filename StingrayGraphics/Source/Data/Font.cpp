#include "Font.h"
#include <dwrite.h>

#include <assert.h>

#define HR(hr) do { HRESULT _hr = (hr); assert(SUCCEEDED(_hr)); } while (0)

struct SRFontLoader {
	IDWriteFactory* dwrite_factory;
};

SRFontLoader* SRFontLoader_Create(SRArena* arena) {
	SRFontLoader* loader = SRArena_PushStructZero(arena, SRFontLoader);

	HR(DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		(IUnknown**)&loader->dwrite_factory
	));

	return loader;
}

void SRFontLoader_Destroy(SRFontLoader* font_loader) {
	font_loader->dwrite_factory->Release();
}

void SRFontLoader_LoadFontFromSystem(SRFontLoader* font_loader, const char* name, SRFont* font) {
	IDWriteTextFormat* dwrite_text_format = nullptr;

	HR(font_loader->dwrite_factory->CreateTextFormat(
		L"Segoe UI",
		nullptr,
		DWRITE_FONT_WEIGHT_REGULAR,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		72.0f,
		L"en-us",
		&dwrite_text_format
	));
}

void SRFont_Create(SRFont& font) {

}
