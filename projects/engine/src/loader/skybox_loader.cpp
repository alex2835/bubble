#pragma once
#include "engine/loader/loader.hpp"

namespace bubble
{
Ref<Skybox> Loader::JustLoadSkybox( const path& path )
{
    auto [data, spec] = OpenImage( path );

    auto width = spec.mWidth / 4;
    auto height = spec.mHeight / 3;
    auto channels = spec.ExtractTextureSpecChannels();

    std::array<Scope<u8[]>, 6> newData;
    for ( auto& val : newData )
        val = CreateScope<u8[]>( width * height * channels );

    Texture2DSpecification newSpec = spec;
    newSpec.mWidth = width;
    newSpec.mHeight = height;
    
    // Parse 6 textures from single image
    // 0-left 1-front 2-right 3-back
    for ( u32 i = 0; i < 4; i++ )
    {
        for ( u32 y = 0; y < height; y++ )
        {
            u32 yOffset = height;
            u32 yRawCoord = ( y + yOffset ) * spec.mWidth * channels;
            u32 rawWidth = width * channels;
            memmove( &newData[i][y * rawWidth], &data[yRawCoord + rawWidth * i], rawWidth );
        }
    }
    // 4-top
    for ( u32 y = 0; y < height; y++ )
    {
        u32 yOffset = height * 2;
        u32 yRawCoord = ( y + yOffset ) * spec.mWidth * channels;
        u32 rawWidth = width * channels;
        memmove( &newData[4][y * rawWidth], &data[yRawCoord + rawWidth], rawWidth );
    }
    // 5-bottom
    for ( u32 y = 0; y < height; y++ )
    {
        u32 yRawCoord = y * spec.mWidth * channels;
        u32 rawWidth = width * channels;
        memmove( &newData[5][y * rawWidth], &data[yRawCoord + rawWidth], rawWidth );
    }
    // Reorder to: right, left, top, bottom, front, back
    std::swap( data[0], data[2] );
    std::swap( data[1], data[2] );
    std::swap( data[2], data[4] );
    std::swap( data[3], data[5] );
    std::swap( data[2], data[3] );

    Ref<Skybox> skybox = CreateRef<Skybox>( Cubemap( newData, spec ) );
    return skybox;
}

Ref<Skybox> Loader::LoadSkybox( const path& path )
{
    auto iter = mSkyboxes.find( path );
    if ( iter != mSkyboxes.end() )
        return iter->second;

    auto skybox = JustLoadSkybox( path );
    mSkyboxes.emplace( path, skybox );
    return skybox;
}


//Ref<Skybox> Loader::LoadSkyboxFromDir( const std::string& dir, const std::string& ext )
//{
//    Ref<Skybox> skybox = CreateRef<Skybox>();
//    skybox->mSkybox = Cubemap( dir, ext );
//    mLoadedSkyboxes.emplace( dir, skybox );
//    return skybox;
//}

}
