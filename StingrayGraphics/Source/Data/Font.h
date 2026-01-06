#pragma once

#include "Core/Types.h"
#include "Data/ArenaAllocator.h"

struct SRFontInfo {
	u32 size;

};

struct SRFont {
	SRFontInfo info;
};

struct SRFontLoader;

SRFontLoader* SRFontLoader_Create(SRArena* arena);
void          SRFontLoader_Destroy(SRFontLoader* font_loader);
void          SRFontLoader_LoadFontFromSystem(SRFontLoader* font_loader, const char* name, SRFont* font);