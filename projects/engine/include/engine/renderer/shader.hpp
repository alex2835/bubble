#pragma once
#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_containers.hpp>
#include "engine/utils/error.hpp"
#include "engine/types/number.hpp"
#include "engine/types/pointer.hpp"
#include "engine/types/string.hpp"
#include "engine/types/map.hpp"
#include "engine/utils/filesystem.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/buffer.hpp"

namespace bubble
{
using UniformDescription = map<string, GLSLDataType>;

// The value a uniform holds in the freshly linked program - whatever its GLSL
// initializer said, or zero when it declared none. Read once at load time
// rather than on demand: shaders are cached and shared between entities, so by
// the time anyone asks, a draw call has long since overwritten the live value.
//
// One slot big enough for a mat4 covers every float-family type, and four ints
// cover int/bool/ivec. A sampler has no meaningful default and is not recorded.
struct UniformDefault
{
    array<f32, 16> mFloats{};
    array<i32, 4> mInts{};
};
using UniformDefaults = map<string, UniformDefault>;

enum class ShaderModule
{
    None,
    Material,
    Light,
};
using ShaderModules = magic_enum::containers::bitset<ShaderModule>;


class Shader
{
public:
    Shader() = default;

    Shader( const Shader& ) = delete;
    Shader& operator= ( const Shader& ) = delete;

    Shader( Shader&& ) noexcept;
    Shader& operator= ( Shader&& ) noexcept;

    void Swap( Shader& other ) noexcept;

    i32 GetUniform( string_view name ) const;
    i32 GetUniformBuffer( string_view name ) const;

    // Silent existence check. GetUniform warns on a miss, which is what you
    // want for a uniform the renderer requires and wrong for one a caller
    // merely offers - a material field no shader reads is stripped by the GLSL
    // compiler and is legitimately absent from the linked program.
    bool HasUniform( string_view name ) const;

    void Bind() const;
    void Unbind() const;

    // lone i32
    void SetUni1i( string_view name, const i32& val ) const;
    void SetUni1u( string_view name, const u32& val ) const;

    // i32 vec
    void SetUni2i( string_view name, const ivec2& val ) const;
    void SetUni3i( string_view name, const ivec3& val ) const;
    void SetUni4i( string_view name, const ivec4& val ) const;

    // f32 vec
    void SetUni1f( string_view name, const f32& val ) const;
    void SetUni2f( string_view name, const vec2& val ) const;
    void SetUni3f( string_view name, const vec3& val ) const;
    void SetUni4f( string_view name, const vec4& val ) const;

    // f32 matrices
    void SetUniMat3( string_view name, const mat3& val ) const;
    void SetUniMat4( string_view name, const mat4& val ) const;

    // textures
    void SetTexture2D( string_view name, i32 tex_id, i32 slot = 0 ) const;
    void SetTexture2D( string_view name, const Texture2D& texture, i32 slot = 0 ) const;
    void SetTexture2D( string_view name, const Ref<Texture2D>& texture, i32 slot = 0 ) const;

    // Uniform buffer
    void SetUniformBuffer( const Ref<UniformBuffer>& ub );

    string mName;
    path mPath;
    ShaderModules mModules;
    u32 mShaderId = 0;
    UniformDescription mUniformDescriptors;
    UniformDefaults mUniformDefaults;
private:
    i32 LookupUniform( string_view name, bool warnIfMissing ) const;

    // Uniform locations and uniform block indices are separate namespaces in
    // GL, so they cannot share one cache: a block and a uniform with the same
    // name would hand each other's index out.
    mutable str_hash_map<i32> mUniformCache;
    mutable str_hash_map<i32> mUniformBlockCache;
};

}