
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

int Shader::GetUni( const std::string& uniformname ) const
{
    glcall( glUseProgram( mShaderID ) );
    if( mUniformCache.count( uniformname ) )
        return mUniformCache[uniformname];

    int unifromid = glGetUniformLocation( mShaderID, uniformname.c_str() );
    if( unifromid == -1 )
        LogError( "Shader: {0} doesn't have uniform: {1}", mName, uniformname );

    mUniformCache[uniformname] = unifromid;
    return unifromid;
}

// lone int 
void Shader::SetUni1i( const std::string& mName, const int& val ) const
{
    glcall( glUniform1i( GetUni( mName ), val ) );
}

// float vec
void Shader::SetUni1f( const std::string& mName, const float& val ) const
{
    glcall( glUniform1f( GetUni( mName ), val ) );
}

void Shader::SetUni2f( const std::string& mName, const glm::vec2& val ) const
{
    glcall( glUniform2f( GetUni( mName ), val.x, val.y ) );
}

void Shader::SetUni3f( const std::string& mName, const glm::vec3& val ) const
{
    glcall( glUniform3f( GetUni( mName ), val.x, val.y, val.z ) );
}

void Shader::SetUni4f( const std::string& mName, const glm::vec4& val ) const
{
    glcall( glUniform4f( GetUni( mName ), val.x, val.y, val.z, val.w ) );
}

// float matrices
void Shader::SetUniMat3( const std::string& mName, const glm::mat3& val ) const
{
    glcall( glUniformMatrix3fv( GetUni( mName ), 1, GL_FALSE, glm::value_ptr( val ) ) );
}

void Shader::SetUniMat4( const std::string& mName, const glm::mat4& val ) const
{
    glcall( glUniformMatrix4fv( GetUni( mName ), 1, GL_FALSE, glm::value_ptr( val ) ) );
}

// Texture
void Shader::SetTexture2D( const std::string& name, int tex_id, int slot ) const
{
    glcall( glActiveTexture( GL_TEXTURE0 + slot ) );
    glcall( glBindTexture( GL_TEXTURE_2D, tex_id ) );
    SetUni1i( name, slot );
}

void Shader::SetTexture2D( const std::string& name, const Texture2D& texture, int slot ) const
{
    glcall( glActiveTexture( GL_TEXTURE0 + slot ) );
    glcall( glBindTexture( GL_TEXTURE_2D, texture.GetRendererID() ) );
    SetUni1i( name, slot );
}

void Shader::SetTexture2D( const std::string& name, const Ref<Texture2D>& texture, int slot ) const
{
    glcall( glActiveTexture( GL_TEXTURE0 + slot ) );
    glcall( glBindTexture( GL_TEXTURE_2D, texture->GetRendererID() ) );
    SetUni1i( name, slot );
}

}