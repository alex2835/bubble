#pragma once
#include "bubble/import/importer.hpp"

namespace bubble
{
#ifdef BUBBLE_IMPORT_FONTS
// .ttf and .otf, kind "font" (bubble/assets/font_asset.hpp): the glyphs read
// by FreeType, each drawn as a multi-channel signed distance field by
// msdfgen and packed into one atlas, with the font's metrics and kerning.
// The settings, every one may be left out:
//
//     { "size": 48,     atlas pixels an em is drawn at
//       "range": 4,     how many of them the distance spans, both ways
//       "chars": "" }   characters beyond the usual: Latin with Latin-1,
//                       Cyrillic, the common punctuation
//
// A character the font lacks is left out; the text shows nothing for it.
// Not on the web - BUBBLE_IMPORT_FONTS -, as models are not.
Importer FontImporter( string_view extension );
#endif
}
