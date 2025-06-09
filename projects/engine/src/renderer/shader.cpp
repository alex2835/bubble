#include "engine/pch/pch.hpp"
#include <GL/glew.h>
#include "engine/log/log.hpp"
#include "engine/renderer/shader.hpp"

namespace bubble
{
Shader::Shader( Shader&& other ) noexcept
    : mName( std::move( other.mName ) ),
      mShaderId( other.mShaderId ),
      mUniformCache( std::move( other.mUniformCache ) )
{
    other.mShaderId = 0;
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
    std::swap( mPath, other.mPath );
    std::swap( mModules, other.mModules );
}

i32 Shader::GetUniform( string_view uniformName ) const
{
    glcall( glUseProgram( mShaderId ) );
    auto iter = mUniformCache.find( uniformName );
    if( iter != mUniformCache.end() )
        return iter->second;

    i32 uniformId = glGetUniformLocation( mShaderId, uniformName.data() );
    if( uniformId == GL_INVALID_INDEX )
        LogWarning( "Shader {} doesn't have uniform: {}", mName, uniformName );

    mUniformCache.emplace( uniformName, uniformId );
    return uniformId;
}

i32 Shader::GetUniformBuffer( string_view uniformName ) const
{
    glcall( glUseProgram( mShaderId ) );
    auto iter = mUniformCache.find( uniformName );
    if ( iter != mUniformCache.end() )
        return iter->second;

    i32 uniformId = glGetUniformBlockIndex( mShaderId, uniformName.data() );
    if ( uniformId == GL_INVALID_INDEX )
        LogWarning( "Shader {} doesn't have uniform buffer: {}", mName, uniformName );

    mUniformCache.emplace( uniformName, uniformId );
    return uniformId;
}

// lone i32 
void Shader::SetUni1i( string_view name, const i32& val ) const
{
    glcall( glUniform1i( GetUniform( name ), val ) );
}

void Shader::SetUni1ui( string_view name, const u32& val ) const
{
    glcall( glUniform1ui( GetUniform( name ), val ) );
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