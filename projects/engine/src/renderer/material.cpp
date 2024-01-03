
#include "engine/renderer/material.hpp"

namespace bubble
{
ColorMaterial::ColorMaterial( const glm::vec3& ambient,
                              const glm::vec3& diffuse,
                              const glm::vec3& specular,
                              int shininess )
    : mAmbient( ambient ),
      mDiffuse( diffuse ),
      mSpecular( specular ),
      mShininess( shininess )
{
}

void ColorMaterial::Set( const Ref<Shader>& shader )
{

}


BasicMaterial::BasicMaterial( const Ref<Texture2D>& diffuse_map,
                                  const Ref<Texture2D>& specular_map,
                                  const Ref<Texture2D>& normal_map,
                                  int shininess )
    : mDiffuseMap( diffuse_map ),
      mSpecularMap( specular_map ),
      mNormalMap( normal_map ),
      mShininess( shininess )
{
}

void BasicMaterial::Set( const Ref<Shader>& shader ) const
{
    shader->SetUni4f( "material.diffuse_color", mDiffuseColor );
    shader->SetUni1f( "material.specular_coef", mSpecularCoef );
    shader->SetUni1f( "material.ambient_coef", mAmbientCoef );

    // Maps
    shader->SetUni1i( "material.diffuse_map", 0 );
    mDiffuseMap->Bind( 0 );

    shader->SetUni1i( "material.specular_map", 1 );
    mSpecularMap->Bind( 1 );

    if( mNormalMap )
    {
        shader->SetUni1i( "material.normal_map", 2 );
        mNormalMap->Bind( 2 );
    }

    shader->SetUni1i( "material.shininess", mShininess );
    shader->SetUni1i( "u_NormalMapping", (bool)mNormalMap );
    shader->SetUni1f( "u_NormalMappingStrength", mNormalMapStrength );
}


ExtendedMaterial::ExtendedMaterial( std::vector<Ref<Texture2D>>&& diffuse_maps,
                                    std::vector<Ref<Texture2D>>&& specular_maps,
                                    std::vector<Ref<Texture2D>>&& normal_maps,
                                    int shininess )
    : mDiffuseMaps( std::move( diffuse_maps ) ),
      mSpecularMaps( std::move( specular_maps ) ),
      mNormalMaps( std::move( normal_maps ) ),
      mShininess( shininess )
{
}

void ExtendedMaterial::Set( const Ref<Shader>& shader ) const
{
    int slot = 0;
    for( int i = 0; i < mDiffuseMaps.size(); i++ )
    {
        shader->SetUni1i( "material.diffuse" + std::to_string( i ), slot );
        mDiffuseMaps[i]->Bind( slot++ );
    }

    for( int i = 0; i < mSpecularMaps.size(); i++ )
    {
        shader->SetUni1i( "material.specular" + std::to_string( i ), slot );
        mSpecularMaps[i]->Bind( slot++ );
    }

    for( int i = 0; i < mNormalMaps.size(); i++ )
    {
        shader->SetUni1i( "material.normal" + std::to_string( i ), slot );
        mNormalMaps[i]->Bind( slot++ );
    }

    shader->SetUni1i( "material.shininess", mShininess );
    shader->SetUni1i( "u_NormalMapping", static_cast<int>( mNormalMaps.size() ) );
}

}