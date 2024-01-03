#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>
#include <exception>
#include "glm/glm.hpp"
#include <glm/gtc/type_ptr.hpp>
#include "engine/utils/error.hpp"
#include "engine/utils/pointers.hpp"
#include "engine/utils/imexp.hpp"
#include "engine/renderer/texture.hpp"

namespace bubble
{
class BUBBLE_ENGINE_EXPORT Shader
{
public:
    Shader() = default;

    Shader( const Shader& ) = delete;
    Shader& operator= ( const Shader& ) = delete;

    Shader( Shader&& ) noexcept;
    Shader& operator= ( Shader&& ) noexcept;

    int GetUni( const std::string& name ) const;

    void Bind() const;
    void Unbind() const;

    // lone int 
    void SetUni1i( const std::string& name, const int& val ) const;

    // float vec
    void SetUni1f( const std::string& name, const float& val ) const;
    void SetUni2f( const std::string& name, const glm::vec2& val ) const;
    void SetUni3f( const std::string& name, const glm::vec3& val ) const;
    void SetUni4f( const std::string& name, const glm::vec4& val ) const;

    // float matrices
    void SetUniMat3( const std::string& name, const glm::mat3& val ) const;
    void SetUniMat4( const std::string& name, const glm::mat4& val ) const;

    // textures
    void SetTexture2D( const std::string& name, int tex_id, int slot = 0 ) const;
    void SetTexture2D( const std::string& name, const Texture2D& texture, int slot = 0 ) const;
    void SetTexture2D( const std::string& name, const Ref<Texture2D>& texture, int slot = 0 ) const;

//private:
    std::string mName;
    GLuint  mShaderID;
    mutable std::unordered_map<std::string, int> mUniformCache;
};

}