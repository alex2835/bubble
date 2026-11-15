#pragma once
#include "bubble/import/importer.hpp"

namespace bubble
{
// Images into textures, kind "texture" (bubble/assets/texture_asset.hpp):
// .png, .jpg, .jpeg, .tga, .bmp decoded to RGBA8, and the mip levels made
// down to 1×1, each pixel the mean of the four above it - in linear light
// for a color image. The settings, every one may be left out:
//
//     { "srgb": true,    colors as a person paints them; false for data -
//                        normals, masks - averaged as stored
//       "mips": true }   the levels below the full size
//
// An image that will not decode is refused with the decoder's reason.
vector<Importer> TextureImporters();
}
