#include "Font.h"

namespace SR {
	int Font::calc_text_width(const std::string& text) const {
		// Edge case
		if (text.length() == 1) {
			return static_cast<int>(glyphs[text[0]].width);
		}

		int result = 0;
		for (size_t i = 0; i < text.length(); ++i) {
			const GlyphData& glyphData = glyphs[text[i]];

			// For the last character we want its full width including bearing
			if (text[i] != ' ' && i == text.length() - 1) {
				result += glyphData.bearingX + glyphData.width;
				continue;
			}

			result += glyphData.advanceX;

			// We disregard the bearing of the first character
			if (i == 0) {
				result -= glyphData.bearingX;
			}
		}

		return result;
	}
}
