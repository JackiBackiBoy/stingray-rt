#include "Font.h"

#include "Core/StringTypes.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <ShlObj.h>

#undef internal

#include <ft2build.h>
#include FT_FREETYPE_H

#define HR(hr) do { HRESULT _hr = (hr); assert(SUCCEEDED(_hr)); } while (0)
#define FT_CHECK(err) do { assert(!err); } while (0)

struct SRFontLoader {
	
};

SRFontLoader* SRFontLoader_Create(SRArena* arena) {
	SRFontLoader* loader = SRArena_PushStructZero(arena, SRFontLoader);
	return loader;
}

void SRFontLoader_Destroy(SRFontLoader* font_loader) {
}

void SRFontLoader_LoadFontFromSystem(SRFontLoader* font_loader, SRGFXDevice* gfx_device, const char* name, u32 size, SRFont* font) {
	SRArena* arena = SRArena_Create(Kilobytes(1)); // TODO: Move arena

	FT_Library ft_library;
	FT_CHECK(FT_Init_FreeType(&ft_library));

	Str16 wide_font_dir = {};
	HR(SHGetKnownFolderPath(FOLDERID_Fonts, KF_FLAG_DEFAULT, nullptr, (WCHAR**)&wide_font_dir.data));
	wide_font_dir.size = wcslen((WCHAR*)wide_font_dir.data) * sizeof(WCHAR);

	Str8 font_dir = Str8_FromStr16(arena, wide_font_dir);
	font_dir = Str8_Concat(arena, font_dir, Str8_Literal("\\"));
	Str8 font_name = Str8_Build((u8*)name, strlen(name));
	Str8 font_path = Str8_Concat(arena, font_dir, font_name);
	font_path = Str8_Concat(arena, font_path, Str8_Literal(".ttf"));

	FT_Face ft_face;
	FT_CHECK(FT_New_Face(ft_library, (char*)font_path.data, 0, &ft_face));

	FT_Set_Pixel_Sizes(ft_face, 0, size);
	font->line_spacing = ft_face->size->metrics.height >> 6;
	font->bbox_ymax = ft_face->bbox.yMax >> 6;

	b32 has_kerning = FT_HAS_KERNING(ft_face);
	u32 atlas_padding = 4;
	u32 atlas_width = 0;
	u32 atlas_height = 0;
	u32 target_atlas_width = 128;
	u32 target_atlas_height = 128;

	while (true) {
		b32 is_atlas_valid = true;
		u32 pen_x = atlas_padding;
		u32 pen_y = atlas_padding;
		u32 tallest_char_in_row = 0;
		f32 inv_target_atlas_width = 1.0f / (f32)target_atlas_width;
		f32 inv_target_atlas_height = 1.0f / (f32)target_atlas_height;

		for (u8 c = 32; c < 127; ++c) {
			SRFontGlyph* glyph = &font->glyphs[c];
			FT_CHECK(FT_Load_Char(ft_face, c, FT_LOAD_RENDER));
			FT_Bitmap* bmp = &ft_face->glyph->bitmap;

			if (c == ' ') {
				glyph->advance_x = ft_face->glyph->advance.x >> 6;
				continue;
			}

			// TODO: Use glyph metrics instead for bearing
			glyph->width = bmp->width;
			glyph->height = bmp->rows;
			glyph->bearing_x = ft_face->glyph->bitmap_left;
			glyph->bearing_y = ft_face->glyph->bitmap_top;
			glyph->advance_x = ft_face->glyph->advance.x >> 6;
			glyph->atlas_x = pen_x;
			glyph->atlas_y = pen_y;
			glyph->atlas_tex_coord_tl.x = (f32)pen_x * inv_target_atlas_width;
			glyph->atlas_tex_coord_tl.y = (f32)pen_y * inv_target_atlas_height;
			glyph->atlas_tex_coord_br.x = (f32)(pen_x + glyph->width) * inv_target_atlas_width;
			glyph->atlas_tex_coord_br.y = (f32)(pen_y + glyph->height) * inv_target_atlas_height;
			
			if (glyph->height > tallest_char_in_row) {
				tallest_char_in_row = glyph->height;
			}

			pen_x += glyph->width + atlas_padding;
			if (pen_x + glyph->width > target_atlas_width - atlas_padding) {
				pen_x = atlas_padding;
				pen_y += tallest_char_in_row + atlas_padding;
				tallest_char_in_row = 0;
			}

			if (pen_y > target_atlas_height - atlas_padding) {
				is_atlas_valid = false;
				break;
			}
		}

		if (is_atlas_valid) {
			break;
		}

		target_atlas_width *= 2;
		target_atlas_height *= 2;
	}

	atlas_width = target_atlas_width;
	atlas_height = target_atlas_height; // TODO: For now we always make POT texture

	u8* font_atlas_tex_data = (u8*)malloc(atlas_width * atlas_height);
	ZeroMemory(font_atlas_tex_data, atlas_width * atlas_height);

	for (u8 c = 33; c < 127; ++c) {
		SRFontGlyph* glyph = &font->glyphs[c];
		FT_CHECK(FT_Load_Char(ft_face, c, FT_LOAD_RENDER));
		FT_Bitmap* bmp = &ft_face->glyph->bitmap;

		for (u32 y = 0; y < glyph->height; ++y) {
			for (u32 x = 0; x < glyph->width; ++x) {
				font_atlas_tex_data[(glyph->atlas_x + x) + (glyph->atlas_y + y) * atlas_width] = bmp->buffer[x + y * bmp->pitch];
			}
		}
	}

	SRSubresourceData font_atlas_tex_subresource = { .data = font_atlas_tex_data, .rowPitch = atlas_width };
	SRTextureInfo font_atlas_tex_info = {
		.width = atlas_width,
		.height = atlas_height,
		.format = SRFormat::R8_UNORM,
		.bindFlags = SRBindFlag::ShaderResource
	};
	SRGFX_CreateTexture(gfx_device, &font_atlas_tex_info, &font->atlas_tex, &font_atlas_tex_subresource);

	free(font_atlas_tex_data);
	CoTaskMemFree(wide_font_dir.data);
	FT_CHECK(FT_Done_FreeType(ft_library));
	SRArena_Destroy(arena);
}

void SRFont_Create(SRFont& font) {

}
