#include "stb_image.h"
#include "engine/loader/loader.hpp"
#include "engine/utils/filesystem.hpp"
#include <future>
#include <ranges>
#include "thread_pool.hpp"
#include "fixed_size_packaged_task.hpp"

namespace bubble
{
TextureData OpenTexture( const path& path )
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
    return { Scope<u8[]>( data ), spec, path };
}

Ref<Texture2D> LoadTexture2D( const path& path )
{
    auto [data, spec, _] = OpenTexture( path );
    Ref<Texture2D> texture = CreateRef<Texture2D>( spec );
    texture->SetData( data.get(), spec.mWidth * spec.mHeight * spec.ExtractTextureSpecChannels() );
    return texture;
}

Ref<Texture2D> LoadTexture2D( const TextureData& textureData )
{
    const auto& [data, spec, _] = textureData;
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


void Loader::LoadTextures2D( const vector<path>& texturePaths )
{
    ThreadPool threadPool;
    std::vector<FixedSizePackagedTask<pair<path, TextureData>()>> textureDataTasks;

    for ( const path& texturePath : texturePaths )
    {
        if ( mTextures.contains( texturePath ) )
            continue;

        textureDataTasks.emplace_back( [texturePath]()
        {
            return std::make_pair( texturePath, OpenTexture( texturePath ) );
        } );
    }
    threadPool.AddTasks( textureDataTasks );

    for ( auto& task : textureDataTasks )
    {
        auto [texturePath, textureData] = task.get();
        mTextures[texturePath] = bubble::LoadTexture2D( textureData );
    }
}

}