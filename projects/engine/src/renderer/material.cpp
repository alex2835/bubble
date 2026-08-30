#include "engine/pch/pch.hpp"
#include "engine/renderer/material.hpp"

namespace bubble
{
void BasicMaterial::Apply( const Ref<Shader>& shader ) const
{
    shader->Bind();

    // A shader that opts into the material module is not obliged to read every
    // field of it, and the GLSL compiler strips whatever nothing reads - so a
    // missing uniform here is by design, not a mistake. Ask before setting, and
    // leave GetUniform's warning for the case it is actually useful for: a name
    // that is simply wrong.
    const auto set4f = [&]( string_view name, const vec4& value )
    {
        if ( shader->HasUniform( name ) )
            shader->SetUni4f( name, value );
    };
    const auto set1i = [&]( string_view name, i32 value )
    {
        if ( shader->HasUniform( name ) )
            shader->SetUni1i( name, value );
    };
    const auto set1f = [&]( string_view name, f32 value )
    {
        if ( shader->HasUniform( name ) )
            shader->SetUni1f( name, value );
    };

    set4f( "uMaterial.diffuseColor", mDiffuseColor );
    set4f( "uMaterial.specularColor", mSpecular );
    set4f( "uMaterial.ambientColor", mAmbient );

    // Textures. The bind still happens when the sampler uniform was stripped -
    // binding a slot nothing samples is harmless, and skipping it would make
    // the active texture units depend on which shader is in use.
    if ( mDiffuseMap )
    {
        set1i( "uMaterial.hasDiffuseMap", true );
        set1i( "uMaterial.diffuseMap", 0 );
        mDiffuseMap->Bind( 0 );
    }
    else
        set1i( "uMaterial.hasDiffuseMap", false );

    if ( mSpecularMap )
    {
        set1i( "uMaterial.hasSpecularMap", true );
        set1i( "uMaterial.specularMap", 1 );
        mSpecularMap->Bind( 1 );
    }
    else
        set1i( "uMaterial.hasSpecularMap", false );

    if( mNormalMap )
    {
        set1i( "uMaterial.hasNormalMap", true );
        set1i( "uMaterial.normalMap", 2 );
        mNormalMap->Bind( 2 );
    }
    else
        set1i( "uMaterial.hasNormalMap", false );

    set1i( "uMaterial.shininess", mShininess );
    set1f( "uMaterial.shininessStrength", mShininessStrength );
    set1i( "uNormalMapping", (bool)mNormalMap );
    set1f( "uNormalMappingStrength", mNormalMapStrength );
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