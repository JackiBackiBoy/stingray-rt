#pragma once

#include "Core/Types.h"
#include "Core/StringTypes.h"
#include "Graphics/GraphicsDevice.h"
#include "Data/ArenaAllocator.h"
#include <glm/glm.hpp>

struct SRFontInfo {
	u32 size;

};

struct SRFontGlyph {
	u32 width;
	u32 height;
	i32 bearing_x;
	i32 bearing_y;
	u32 advance_x;
	glm::vec2 atlas_tex_coord_tl;
	glm::vec2 atlas_tex_coord_br;
	u32 atlas_x;
	u32 atlas_y;
};

struct SRFont {
	SRFontInfo info;
	u32 line_spacing;
	u32 bbox_ymax;
	SRFontGlyph glyphs[128];
	SRTexture atlas_tex;
	u32 bearing_ymax;
};

f32 SRFont_CalcTextWidth(const SRFont* font, Str8 str);

struct SRFontLoader;
SRFontLoader* SRFontLoader_Create(SRArena* arena);
void          SRFontLoader_Destroy(SRFontLoader* font_loader);
void          SRFontLoader_LoadFontFromSystem(SRFontLoader* font_loader, SRGFXDevice* gfx_device, const char* name, u32 size, SRFont* font);