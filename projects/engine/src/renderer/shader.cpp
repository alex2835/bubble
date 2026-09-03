    #include "engine/pch/pch.hpp"
#include <GL/glew.h>
#include "engine/log/log.hpp"
#include "engine/renderer/shader.hpp"

namespace bubble
{
// Via Swap so there is exactly one list of members to keep up to date. The
// hand-written member list this replaced moved three of them and left mPath,
// mModules and mUniformDescriptors behind - a moved-from shader kept its path,
// and the moved-to one had no material and no uniforms.
Shader::Shader( Shader&& other ) noexcept
{
    Swap( other );
}

Shader& Shader::operator=( Shader&& other ) noexcept
{
    if ( this != &other )
        Swap( other );
    return *this;
}

void Shader::Bind() const
{
    glcall( glUseProgram( mShaderId ) );
}

void Shader::Unbind() const
{
    glcall( glUseProgram( 0 ) );
}

void Shader::Swap( Shader& other ) noexcept
{
    std::swap( mShaderId, other.mShaderId );
    std::swap( mName, other.mName );
    std::swap( mUniformCache, other.mUniformCache );
    std::swap( mUniformBlockCache, other.mUniformBlockCache );
    std::swap( mPath, other.mPath );
    std::swap( mModules, other.mModules );
    // Missing here before, and the hot reloader swaps a freshly compiled shader
    // into the live one - so a reloaded shader kept the old descriptor set and
    // a uniform added to the file never reached the inspector.
    std::swap( mUniformDescriptors, other.mUniformDescriptors );
    std::swap( mUniformDefaults, other.mUniformDefaults );
}

i32 Shader::LookupUniform( string_view uniformName, bool warnIfMissing ) const
{
    glcall( glUseProgram( mShaderId ) );
    auto iter = mUniformCache.find( uniformName );
    if( iter != mUniformCache.end() )
        return iter->second;

    // glGetUniformLocation reads a null-terminated string; a string_view is not
    // guaranteed to be one, and the key has to be materialised for the cache
    // anyway.
    string key( uniformName );
    i32 uniformId = glGetUniformLocation( mShaderId, key.c_str() );
    if( uniformId == -1 and warnIfMissing )
        LogWarning( "Shader {} doesn't have uniform: {}", mName, uniformName );

    mUniformCache.emplace( std::move( key ), uniformId );
    return uniformId;
}

i32 Shader::GetUniform( string_view uniformName ) const
{
    return LookupUniform( uniformName, true );
}

bool Shader::HasUniform( string_view uniformName ) const
{
    return LookupUniform( uniformName, false ) != -1;
}

i32 Shader::GetUniformBuffer( string_view uniformName ) const
{
    glcall( glUseProgram( mShaderId ) );
    auto iter = mUniformBlockCache.find( uniformName );
    if ( iter != mUniformBlockCache.end() )
        return iter->second;

    // Same reason as LookupUniform: glGetUniformBlockIndex reads a
    // null-terminated string and a string_view is not guaranteed to be one.
    string key( uniformName );
    i32 uniformId = (i32)glGetUniformBlockIndex( mShaderId, key.c_str() );
    if ( uniformId == (i32)GL_INVALID_INDEX )
        LogWarning( "Shader {} doesn't have uniform buffer: {}", mName, uniformName );

    mUniformBlockCache.emplace( std::move( key ), uniformId );
    return uniformId;
}

// lone i32 
void Shader::SetUni1i( string_view name, const i32& val ) const
{
    glcall( glUniform1i( GetUniform( name ), val ) );
}

void Shader::SetUni1u( string_view name, const u32& val ) const
{
    glcall( glUniform1ui( GetUniform( name ), val ) );
}

// i32 vec
void Shader::SetUni2i( string_view name, const ivec2& val ) const
{
    glcall( glUniform2iv( GetUniform( name ), 1, glm::value_ptr( val ) ) );
}

void Shader::SetUni3i( string_view name, const ivec3& val ) const
{
    glcall( glUniform3iv( GetUniform( name ), 1, glm::value_ptr( val ) ) );
}

void Shader::SetUni4i( string_view name, const ivec4& val ) const
{
    glcall( glUniform4iv( GetUniform( name ), 1, glm::value_ptr( val ) ) );
}

// f32 vec
void Shader::SetUni1f( string_view name, const f32& val ) const
{
    glcall( glUniform1f( GetUniform( name ), val ) );
}

void Shader::SetUni2f( string_view name, const vec2& val ) const
{
    glcall( glUniform2fv( GetUniform( name ), 1, glm::value_ptr( val ) ) );
}

void Shader::SetUni3f( string_view name, const vec3& val ) const
{
    glcall( glUniform3fv( GetUniform( name ), 1, glm::value_ptr( val ) ) );
}

void Shader::SetUni4f( string_view name, const vec4& val ) const
{
    glcall( glUniform4fv( GetUniform( name ), 1, glm::value_ptr( val ) ) );
}

// f32 matrices
void Shader::SetUniMat3( string_view name, const mat3& val ) const
{
    glcall( glUniformMatrix3fv( GetUniform( name ), 1, GL_FALSE, value_ptr( val ) ) );
}

void Shader::SetUniMat4( string_view name, const mat4& val ) const
{
    glcall( glUniformMatrix4fv( GetUniform( name ), 1, GL_FALSE, value_ptr( val ) ) );
}

// Texture
void Shader::SetTexture2D( string_view name, i32 tex_id, i32 slot ) const
{
    glcall( glActiveTexture( GL_TEXTURE0 + slot ) );
    glcall( glBindTexture( GL_TEXTURE_2D, tex_id ) );
    SetUni1i( name, slot );
}

void Shader::SetTexture2D( string_view name, const Texture2D& texture, i32 slot ) const
{
    glcall( glActiveTexture( GL_TEXTURE0 + slot ) );
    glcall( glBindTexture( GL_TEXTURE_2D, texture.RendererID() ) );
    SetUni1i( name, slot );
}

void Shader::SetTexture2D( string_view name, const Ref<Texture2D>& texture, i32 slot ) const
{
    glcall( glActiveTexture( GL_TEXTURE0 + slot ) );
    glcall( glBindTexture( GL_TEXTURE_2D, texture->RendererID() ) );
    SetUni1i( name, slot );
}

void Shader::SetUniformBuffer( const Ref<UniformBuffer>& ub )
{
    auto shaderBufferIndex = GetUniformBuffer( ub->Name() );
    if ( shaderBufferIndex != GL_INVALID_INDEX )
        glcall( glUniformBlockBinding( mShaderId, shaderBufferIndex, (i32)ub->Index() ) );
}

}