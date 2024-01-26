
#include "engine/log/log.hpp"
#include "engine/renderer/shader.hpp"

namespace bubble
{
Shader::Shader( Shader&& other ) noexcept
    : mShaderID( other.mShaderID ),
      mName( std::move( other.mName ) ),
      mUniformCache( std::move( mUniformCache ) )
{
    other.mShaderID = 0;
}

Shader& Shader::operator=( Shader&& other ) noexcept
{
    mShaderID = other.mShaderID;
    mName = std::move( other.mName );
    mUniformCache = std::move( mUniformCache );
    other.mShaderID = 0;
    return *this;
}

void Shader::Bind() const
{
    glcall( glUseProgram( mShaderID ) );
}

void Shader::Unbind() const
{
    glcall( glUseProgram( 0 ) );
}

i32 Shader::GetUniform( const string& uniformname ) const
{
    glcall( glUseProgram( mShaderID ) );
    if( mUniformCache.count( uniformname ) )
        return mUniformCache[uniformname];

    i32 unifromid = glGetUniformLocation( mShaderID, uniformname.c_str() );
    if( unifromid == GL_INVALID_INDEX )
        LogError( "Shader: {0} doesn't have uniform: {1}", mName, uniformname );

    mUniformCache[uniformname] = unifromid;
    return unifromid;
}

i32 Shader::GetUniformBuffer( const string& uniformname ) const
{
    glcall( glUseProgram( mShaderID ) );
    if ( mUniformCache.count( uniformname ) )
        return mUniformCache[uniformname];

    i32 unifromid = glGetUniformBlockIndex( mShaderID, uniformname.c_str() );
    if ( unifromid == GL_INVALID_INDEX )
        LogError( "Shader: {0} doesn't have uniform buffer: {1}", mName, uniformname );

    mUniformCache[uniformname] = unifromid;
    return unifromid;
}

// lone i32 
void Shader::SetUni1i( const string& mName, const i32& val ) const
{
    glcall( glUniform1i( GetUniform( mName ), val ) );
}

// f32 vec
void Shader::SetUni1f( const string& mName, const f32& val ) const
{
    glcall( glUniform1f( GetUniform( mName ), val ) );
}

void Shader::SetUni2f( const string& mName, const vec2& val ) const
{
    glcall( glUniform2f( GetUniform( mName ), val.x, val.y ) );
}

void Shader::SetUni3f( const string& mName, const vec3& val ) const
{
    glcall( glUniform3f( GetUniform( mName ), val.x, val.y, val.z ) );
}

void Shader::SetUni4f( const string& mName, const vec4& val ) const
{
    glcall( glUniform4f( GetUniform( mName ), val.x, val.y, val.z, val.w ) );
}

// f32 matrices
void Shader::SetUniMat3( const string& mName, const mat3& val ) const
{
    glcall( glUniformMatrix3fv( GetUniform( mName ), 1, GL_FALSE, value_ptr( val ) ) );
}

void Shader::SetUniMat4( const string& mName, const mat4& val ) const
{
    glcall( glUniformMatrix4fv( GetUniform( mName ), 1, GL_FALSE, value_ptr( val ) ) );
}

// Texture
void Shader::SetTexture2D( const string& name, i32 tex_id, i32 slot ) const
{
    glcall( glActiveTexture( GL_TEXTURE0 + slot ) );
    glcall( glBindTexture( GL_TEXTURE_2D, tex_id ) );
    SetUni1i( name, slot );
}

void Shader::SetTexture2D( const string& name, const Texture2D& texture, i32 slot ) const
{
    glcall( glActiveTexture( GL_TEXTURE0 + slot ) );
    glcall( glBindTexture( GL_TEXTURE_2D, texture.GetRendererID() ) );
    SetUni1i( name, slot );
}

void Shader::SetTexture2D( const string& name, const Ref<Texture2D>& texture, i32 slot ) const
{
    glcall( glActiveTexture( GL_TEXTURE0 + slot ) );
    glcall( glBindTexture( GL_TEXTURE_2D, texture->GetRendererID() ) );
    SetUni1i( name, slot );
}

void Shader::SetUniformBuffer( const Ref<UniformBuffer>& ub )
{
    auto shaderBufferIndex = GetUniformBuffer( ub->mName );
    if ( shaderBufferIndex != GL_INVALID_INDEX )
    {
        glcall( glUniformBlockBinding( mShaderID, shaderBufferIndex, (GLint)ub->mIndex ) );
    }
}

}