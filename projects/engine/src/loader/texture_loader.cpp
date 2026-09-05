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

    const string pathStr = path.string();

    // WebGPU has no three channel texture format, so a 24 bit image has to be
    // expanded to RGBA on the way in. stbi does that itself when asked for four
    // channels, which is cheaper than doing it here and keeps the uploaded size
    // matching what the specification reports.
    if ( not stbi_info( pathStr.c_str(), &width, &height, &channels ) )
        return std::nullopt;
    const i32 desiredChannels = channels == 3 ? 4 : channels;

    u8* data = stbi_load( pathStr.c_str(), &width, &height, &channels, desiredChannels );
    if ( data == nullptr )
        return std::nullopt;

    auto spec = Texture2DSpecification::CreateRGBA8( { width, height } );
    spec.SetTextureSpecChanels( desiredChannels );
    return TextureData{ Scope<u8[]>( data ), spec, path };
}

Ref<Texture2D> LoadTexture2D( const path& path )
{
    auto textureDataMaybe = OpenTexture( path );
    if ( not textureDataMaybe )
        return nullptr;

    auto [data, spec, _] = std::move( *textureDataMaybe );
    Ref<Texture2D> texture = CreateRef<Texture2D>( spec );
    texture->mName = path.stem().string();
    texture->mPath = path;
    texture->SetData( data.get(), spec.GetTextureSize() );
    return texture;
}

Ref<Texture2D> LoadTexture2D( const TextureData& textureData )
{
    const auto& [data, spec, _] = textureData;
    Ref<Texture2D> texture = CreateRef<Texture2D>( spec );
    texture->mName = textureData.mPath.stem().string();
    texture->mPath = textureData.mPath;
    texture->SetData( data.get(), spec.GetTextureSize() );
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