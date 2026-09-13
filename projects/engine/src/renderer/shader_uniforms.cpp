#include "engine/pch/pch.hpp"
#include "engine/renderer/shader_uniforms.hpp"
#include "engine/renderer/shader.hpp"
#include "engine/renderer/texture.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <sol/sol.hpp>

namespace bubble
{
// Packs a shader's own uniform values into its UserUniforms block.
//
// This replaced a walk over the descriptor set that pushed each value straight
// at the program with glUniform*. WebGPU has no loose uniforms: a value has to
// be written into a buffer at the byte offset the shader gives it, which is what
// the WGSL reflection in the shader loader records in mUniformOffsets.
//
// The contract above this is unchanged. Values still arrive in a Lua table keyed
// by name, a value of the wrong type is still skipped rather than coerced, and a
// uniform the table has no entry for keeps whatever the block already holds.
void PackShaderUniforms( const Shader& shader, const Table& uniforms, vector<u8>& block )
{
    block.assign( shader.mUserUniformSize, 0 );
    if ( block.empty() )
        return;

    const auto write = [&]( u32 offset, const void* data, u64 size )
    {
        if ( offset + size > block.size() )
        {
            BUBBLE_ASSERT( false, "Uniform write past the end of the block" );
            return;
        }
        std::memcpy( block.data() + offset, data, size );
    };

    // A WGSL mat3x3 is three columns each padded to 16 bytes, so it cannot be
    // copied straight out of a glm::mat3.
    const auto writeMat3 = [&]( u32 offset, const mat3& value )
    {
        for ( i32 column = 0; column < 3; column++ )
            write( offset + (u32)column * 16, value_ptr( value[column] ), sizeof( vec3 ) );
    };

    // WGSL has no host shareable bool, so a shader spells one u32.
    const auto writeBool = [&]( u32 offset, bool value )
    {
        const u32 asUint = value ? 1u : 0u;
        write( offset, &asUint, sizeof( asUint ) );
    };

    // Seed with each uniform's declared default before applying the table.
    //
    // A uniform the table has no entry for has to end up at its default, not at
    // zero. Under OpenGL that happened by itself: nothing was written, so the
    // program kept the value its GLSL initializer gave it. Writing into a buffer
    // there is no such fallback, and a defaulted uColor would come out black.
    for ( const auto& [name, type] : shader.mUniformDescriptors )
    {
        if ( type == ShaderDataType::Texture2D )
            continue;

        const auto offsetIt = shader.mUniformOffsets.find( name );
        const auto defaultIt = shader.mUniformDefaults.find( name );
        if ( offsetIt == shader.mUniformOffsets.end() or
             defaultIt == shader.mUniformDefaults.end() )
            continue;

        const u32 offset = offsetIt->second;
        const UniformDefault& value = defaultIt->second;

        switch ( type )
        {
            case ShaderDataType::Float:
            case ShaderDataType::Float2:
            case ShaderDataType::Float3:
            case ShaderDataType::Float4:
                write( offset, value.mFloats.data(),
                       sizeof( f32 ) * ShaderDataComponentCount( type ) );
                break;
            case ShaderDataType::Mat3:
                // Stored row after row in the default, but laid out as three
                // padded columns in the block.
                for ( u32 column = 0; column < 3; column++ )
                    write( offset + column * 16, value.mFloats.data() + column * 3,
                           sizeof( f32 ) * 3 );
                break;
            case ShaderDataType::Mat4:
                write( offset, value.mFloats.data(), sizeof( f32 ) * 16 );
                break;
            case ShaderDataType::Int:
            case ShaderDataType::Int2:
            case ShaderDataType::Int3:
            case ShaderDataType::Int4:
            case ShaderDataType::UInt:
            case ShaderDataType::Bool:
                write( offset, value.mInts.data(),
                       sizeof( i32 ) * ShaderDataComponentCount( type ) );
                break;
            default:
                break;
        }
    }

    for ( const auto& [name, type] : shader.mUniformDescriptors )
    {
        // A sampler is not part of the block. User textures still need their own
        // bindings, which group 3 does not carry yet.
        if ( type == ShaderDataType::Texture2D )
            continue;

        const auto offsetIt = shader.mUniformOffsets.find( name );
        if ( offsetIt == shader.mUniformOffsets.end() )
            continue;
        const u32 offset = offsetIt->second;

        sol::object val = uniforms[name];
        if ( !val.valid() || val.is<sol::nil_t>() )
            continue;

        switch ( type )
        {
            case ShaderDataType::Float:
                if ( val.is<float>() )
                {
                    const f32 value = val.as<float>();
                    write( offset, &value, sizeof( value ) );
                }
                break;
            case ShaderDataType::Float2:
                if ( val.is<vec2>() )
                {
                    const vec2 value = val.as<vec2>();
                    write( offset, value_ptr( value ), sizeof( value ) );
                }
                break;
            case ShaderDataType::Float3:
                if ( val.is<vec3>() )
                {
                    const vec3 value = val.as<vec3>();
                    write( offset, value_ptr( value ), sizeof( value ) );
                }
                break;
            case ShaderDataType::Float4:
                if ( val.is<vec4>() )
                {
                    const vec4 value = val.as<vec4>();
                    write( offset, value_ptr( value ), sizeof( value ) );
                }
                break;
            case ShaderDataType::Mat3:
                if ( val.is<mat3>() )
                    writeMat3( offset, val.as<mat3>() );
                break;
            case ShaderDataType::Mat4:
                if ( val.is<mat4>() )
                {
                    const mat4 value = val.as<mat4>();
                    write( offset, value_ptr( value ), sizeof( value ) );
                }
                break;
            case ShaderDataType::Int:
                if ( val.is<int>() )
                {
                    const i32 value = val.as<int>();
                    write( offset, &value, sizeof( value ) );
                }
                break;
            case ShaderDataType::UInt:
                if ( val.is<int>() )
                {
                    const u32 value = (u32)val.as<int>();
                    write( offset, &value, sizeof( value ) );
                }
                break;
            case ShaderDataType::Bool:
                // RebuildUniforms defaults a bool uniform to Lua `false`, and
                // the inspector edits it as a checkbox - neither of which is a
                // Lua number, so an is<int>() test would never match and bool
                // uniforms would never reach the shader at all.
                if ( val.is<bool>() )
                    writeBool( offset, val.as<bool>() );
                break;
            case ShaderDataType::Int2:
                if ( val.is<ivec2>() )
                {
                    const ivec2 value = val.as<ivec2>();
                    write( offset, value_ptr( value ), sizeof( value ) );
                }
                break;
            case ShaderDataType::Int3:
                if ( val.is<ivec3>() )
                {
                    const ivec3 value = val.as<ivec3>();
                    write( offset, value_ptr( value ), sizeof( value ) );
                }
                break;
            case ShaderDataType::Int4:
                if ( val.is<ivec4>() )
                {
                    const ivec4 value = val.as<ivec4>();
                    write( offset, value_ptr( value ), sizeof( value ) );
                }
                break;
            default:
                break;
        }
    }
}

} // namespace bubble
