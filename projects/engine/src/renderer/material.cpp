
#include "engine/renderer/material.hpp"

namespace bubble
{
void BasicMaterial::Apply( const Ref<Shader>& shader ) const
{
    shader->Bind();
    shader->SetUni4f( "uMaterial.diffuseColor", mDiffuseColor );
    shader->SetUni4f( "uMaterial.specularColor", mSpecular );
    shader->SetUni4f( "uMaterial.ambientColor", mAmbient );

    // Textures
    if ( mDiffuseMap )
    {
        shader->SetUni1i( "uMaterial.hasDiffuseMap", true );
        shader->SetUni1i( "uMaterial.diffuseMap", 0 );
        mDiffuseMap->Bind( 0 );
    }
    else
        shader->SetUni1i( "uMaterial.hasDiffuseMap", false );

    if ( mSpecularMap )
    {
        shader->SetUni1i( "uMaterial.hasSpecularMap", true );
        shader->SetUni1i( "uMaterial.specularMap", 1 );
        mSpecularMap->Bind( 1 );
    }
    else
        shader->SetUni1i( "uMaterial.hasSpecularMap", false );

    if( mNormalMap )
    {
        shader->SetUni1i( "uMaterial.hasNormalMap", true );
        shader->SetUni1i( "uMaterial.normalMap", 2 );
        mNormalMap->Bind( 2 );
    }
    else
        shader->SetUni1i( "uMaterial.hasNormalMap", false );

    shader->SetUni1i( "uMaterial.shininess", mShininess );
    shader->SetUni1f( "uMaterial.shininessStrength", mShininessStrength );
    shader->SetUni1i( "uNormalMapping", (bool)mNormalMap );
    shader->SetUni1f( "uNormalMappingStrength", mNormalMapStrength );
}



//ExtendedMaterial::ExtendedMaterial( vector<Ref<Texture2D>>&& diffuseMaps,
//                                    vector<Ref<Texture2D>>&& specularMaps,
//                                    vector<Ref<Texture2D>>&& normalMaps,
//                                    i32 shininess )
//    : mDiffuseMaps( std::move( diffuseMaps ) ),
//      mSpecularMaps( std::move( specularMaps ) ),
//      mNormalMaps( std::move( normalMaps ) ),
//      mShininess( shininess )
//{
//}
//
//void ExtendedMaterial::Set( const Ref<Shader>& shader ) const
//{
//    i32 slot = 0;
//    for( i32 i = 0; i < mDiffuseMaps.size(); i++ )
//    {
//        shader->SetUni1i( "uMaterial.diffuse" + std::to_string( i ), slot );
//        mDiffuseMaps[i]->Bind( slot++ );
//    }
//
//    for( i32 i = 0; i < mSpecularMaps.size(); i++ )
//    {
//        shader->SetUni1i( "uMaterial.specular" + std::to_string( i ), slot );
//        mSpecularMaps[i]->Bind( slot++ );
//    }
//
//    for( i32 i = 0; i < mNormalMaps.size(); i++ )
//    {
//        shader->SetUni1i( "uMaterial.normal" + std::to_string( i ), slot );
//        mNormalMaps[i]->Bind( slot++ );
//    }
//
//    shader->SetUni1i( "uMaterial.shininess", mShininess );
//    shader->SetUni1i( "u_NormalMapping", static_cast<i32>( mNormalMaps.size() ) );
//}

}