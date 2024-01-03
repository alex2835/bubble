#include "stb_image.h"
#include "engine/loader/loader.hpp"
#include "engine/utils/filesystem.hpp"


namespace bubble
{

//Ref<Texture2D> Loader::LoadAndCacheTexture2D( std::string path, const Texture2DSpecification& spec )
//{
//    //if ( !mProject.Valid() )
//    //    BUBBLE_ASSERT( false, "Try to load and cache texture with not valid project" );
//
//    auto rel_path = CreateRelPath( mProject.GetPath(), path );
//    if ( mLoadedTextures.count( rel_path ) )
//        return mLoadedTextures[rel_path];
//
//    Ref<Texture2D> texture = LoadTexture2D( path, spec );
//    mLoadedTextures.emplace( rel_path, texture );
//    return texture;
//}

Ref<Texture2D> Loader::LoadTexture2D( const std::path& path )
{
    stbi_uc* data = nullptr;
    int width, height, channels;
    stbi_set_flip_vertically_on_load( false );
    data = stbi_load( path.string().c_str(), &width, &height, &channels, 0 );

    if ( data == nullptr )
        throw std::runtime_error( std::format( "Failed to load image: {}", path.string() ) );

    Ref<Texture2D> texture = CreateRef<Texture2D>();
    texture->mSpecification.mWidth = width;
    texture->mSpecification.mHeight = height;
    texture->mSpecification.SetTextureSpecChanels( channels );
    texture->mSpecification.mAnisotropicFiltering = true;
    texture->Invalidate();
    texture->SetData( data, width * height * channels );
    stbi_image_free( data );
    return texture;
}

//std::tuple<Scope<uint8_t[]>, Texture2DSpecification>
//Loader::OpenRawImage( const std::path& path )
//{
//    uint8_t* data = nullptr;
//    int width, height, channels;
//
//    stbi_set_flip_vertically_on_load( false );
//    data = stbi_load( path.string().c_str(), &width, &height, &channels, 0);
//
//    if ( data == nullptr )
//        throw std::runtime_error( std::format( "Failed to load image: {}", path.string() ) );
//
//    Texture2DSpecification spec;
//    spec.mWidth = width;
//    spec.mHeight = height;
//    spec.SetTextureSpecChanels( channels );
//
//    return { Scope<uint8_t[]>( data ), spec };
//}

// Skysphere texture
//Ref<Texture2D> Loader::LoadSkysphere( const std::string& path )
//{
//    if ( mLoadedSkyspheres.count( path ) )
//        return mLoadedSkyspheres[path];
//
//    mLoadedSkyspheres[path] = LoadTexture2D( path );
//    return mLoadedSkyspheres[path];
//}

}