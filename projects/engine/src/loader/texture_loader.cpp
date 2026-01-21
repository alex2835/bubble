#include "engine/pch/pch.hpp"
#include "stb_image.h"
#include "engine/loader/loader.hpp"
#include "engine/utils/filesystem.hpp"
#include "thread_pool.hpp"
#include "fixed_size_packaged_task.hpp"
#include <future>
#include <ranges>

namespace bubble
{
std::optional<TextureData> OpenTexture( const path& path )
{
    i32 width = 0;
    i32 height = 0;
    i32 channels = 0;
    stbi_set_flip_vertically_on_load( false );

    u8* data = stbi_load( path.string().c_str(), &width, &height, &channels, 0 );
    if ( data == nullptr )
        return std::nullopt;

    auto spec = Texture2DSpecification::CreateRGBA8( { width, height } );
    spec.SetTextureSpecChanels( channels );
    return TextureData{ Scope<u8[]>( data ), spec, path };
}

Ref<Texture2D> LoadTexture2D( const path& path )
{
    auto textureDataMaybe = OpenTexture( path );
    if ( not textureDataMaybe )
        return nullptr;

    auto [data, spec, _] = std::move( *textureDataMaybe );
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

Ref<Texture2D> Loader::LoadTexture2D( const path& texturePath )
{
    auto [relPath, absPath] = RelAbsFromProjectPath( texturePath );

    auto iter = mTextures.find( relPath );
    if ( iter != mTextures.end() )
        return iter->second;

    auto texture = bubble::LoadTexture2D( absPath );
    if ( not texture )
    {
        LogError( "Failed to load texture: {}", texturePath.string() );
        return nullptr;
    }

    mTextures.emplace( relPath, texture );
    return texture;
}


void Loader::LoadTextures2D( const vector<path>& texturePaths )
{
    ThreadPool threadPool;
    std::vector<FixedSizePackagedTask<pair<path, std::optional<TextureData>>(), 128>> textureDataTasks;

    for ( const path& texturePath : texturePaths )
    {
        auto [relPath, absPath] = RelAbsFromProjectPath( texturePath );

        if ( mTextures.contains( texturePath ) )
            continue;

        textureDataTasks.emplace_back( [=]()
        {
            return std::make_pair( relPath, OpenTexture( absPath ) );
        } );
    }
    threadPool.AddTasks( textureDataTasks );

    for ( auto& task : textureDataTasks )
    {
        auto [relTexturePath, textureData] = task.get();
        if ( not textureData )
        {
            LogError( "Failed to load texture: {}", relTexturePath.string() );
            continue;
        }
        mTextures[relTexturePath] = bubble::LoadTexture2D( *textureData );
    }
}

}