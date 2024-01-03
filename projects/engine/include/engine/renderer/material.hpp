#pragma once
#include "glm/glm.hpp"
#include "engine/utils/imexp.hpp"
#include "engine/utils/pointers.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/shader.hpp"

namespace bubble
{
struct BUBBLE_ENGINE_EXPORT ColorMaterial
{
    glm::vec3 mAmbient;
    glm::vec3 mDiffuse;
    glm::vec3 mSpecular;
    int mShininess;

    ColorMaterial( const glm::vec3& ambient,
                   const glm::vec3& diffuse,
                   const glm::vec3& specular,
                   int shininess = 32 );

    void Set( const Ref<Shader>& shader );
};


struct BUBBLE_ENGINE_EXPORT BasicMaterial
{
    Ref<Texture2D> mDiffuseMap;
    Ref<Texture2D> mSpecularMap;
    Ref<Texture2D> mNormalMap;
    glm::vec4 mDiffuseColor = glm::vec4( 1.0f );
    float mAmbientCoef = 0.1f;
    float mSpecularCoef = 0.1f;
    int   mShininess = 32;
    float mNormalMapStrength = 0.1f;

    BasicMaterial() = default;
    BasicMaterial( const Ref<Texture2D>& diffuse_map,
                   const Ref<Texture2D>& specular_map,
                   const Ref<Texture2D>& normal_map,
                   int shininess = 32 );

    BasicMaterial( const BasicMaterial& ) = delete;
    BasicMaterial& operator=( const BasicMaterial& ) = delete;

    BasicMaterial( BasicMaterial&& ) = default;
    BasicMaterial& operator=( BasicMaterial&& ) = default;

    void Set( const Ref<Shader>& shader ) const;
};


struct BUBBLE_ENGINE_EXPORT ExtendedMaterial
{
    std::vector<Ref<Texture2D>> mDiffuseMaps;
    std::vector<Ref<Texture2D>> mSpecularMaps;
    std::vector<Ref<Texture2D>> mNormalMaps;
    int mShininess = 32;

    ExtendedMaterial() = default;
    ExtendedMaterial( std::vector<Ref<Texture2D>>&& diffuse_maps,
                      std::vector<Ref<Texture2D>>&& specular_maps,
                      std::vector<Ref<Texture2D>>&& normal_maps,
                      int shininess = 32 );

    ExtendedMaterial( const ExtendedMaterial& ) = delete;
    ExtendedMaterial& operator=( const ExtendedMaterial& ) = delete;

    ExtendedMaterial( ExtendedMaterial&& ) = default;
    ExtendedMaterial& operator=( ExtendedMaterial&& ) = default;

    void Set( const Ref<Shader>& shader ) const;
};


// PBR material
// ...

}