#pragma once
#include "engine/types/any.hpp"
#include "engine/types/array.hpp"
#include "engine/types/number.hpp"

namespace bubble
{
class Shader;

// Packs a shader's own uniform values into the byte block its
// UserUniforms struct describes, ready to be pushed to the GPU.
void PackShaderUniforms( const Shader& shader, const Table& uniforms, vector<u8>& block );

}
