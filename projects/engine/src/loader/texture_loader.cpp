#include "stb_image.h"
#include "engine/loader/loader.hpp"
#include "engine/utils/filesystem.hpp"

namespace bubble
{
std::pair<Scope<u8[]>, Texture2DSpecification> OpenImage(const path& path)
{
    i32 width = 0;
    i32 height = 0;
    i32 channels = 0;
    stbi_set_flip_vertically_on_load( false );
    u8* data = stbi_load( path.string().c_str(), &width, &height, &channels, 0 );

    if ( data == nullptr )
        throw std::runtime_error( std::format( "Failed to load image: {}", path.string() ) );

    auto spec = Texture2DSpecification::CreateRGBA8( { width, height } );
    spec.SetTextureSpecChanels( channels );
    return { Scope<u8[]>( data ), spec };
}

Ref<Texture2D> LoadTexture2D( const path& path )
{
    auto [data, spec] = OpenImage( path );
    Ref<Texture2D> texture = CreateRef<Texture2D>( spec );
    texture->SetData( data.get(), spec.mWidth * spec.mHeight * spec.ExtractTextureSpecChannels() );
    return texture;
}

Ref<Texture2D> Loader::LoadTexture2D( const path& path )
{
    auto iter = mTextures.find( path );
    if ( iter != mTextures.end() )
        return iter->second;

    auto texture = bubble::LoadTexture2D( path );
    mTextures.emplace( path, texture );
    return texture;
}

}