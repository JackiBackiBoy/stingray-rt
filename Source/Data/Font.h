#pragma once

#include "Graphics/GraphicsTypes.h"
#include <glm/glm.hpp>
#include <string>

namespace SR {
	struct GlyphData {
		unsigned int width = 0;
		unsigned int height = 0;
		int bearingX = 0;
		int bearingY = 0;
		int advanceX = 0;
		int advanceY = 0;
		glm::vec2 texCoords[4] = {};
	};

	struct Font {
		std::string name = "";
		float size = 0.0f;
		int maxBearingY = 0;
		int boundingBoxHeight = 0;
		int lineSpacing = 0;
		GlyphData glyphs[128] = {}; // TODO: Only compatible with ASCII at the moment
		Texture atlasTexture = {};

		int calc_text_width(const std::string& text) const;
	};
}
