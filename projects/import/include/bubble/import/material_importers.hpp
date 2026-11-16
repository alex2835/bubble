#pragma once
#include "bubble/import/importer.hpp"

namespace bubble
{
// .wgsl, kind "shader": a material's own WGSL, kept as it is once it keeps
// the contract (bubble/render/material_layout.hpp). Whether it compiles the
// GPU says when it is first drawn.
Importer ShaderImporter();
// .mat, kind "material": the file read, checked and written again as the
// engine reads it. Whether its parameters fit its shader is known when the
// shader is: when it is drawn.
Importer MaterialImporter();
}
