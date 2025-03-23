#pragma once
#include "engine/log/log.hpp"
#include "engine/types/string.hpp"
#include "engine/types/pointer.hpp"
#include "engine/types/map.hpp"
#include "engine/types/utility.hpp"
#include "engine/utils/filesystem.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/shader.hpp"
#include "engine/renderer/model.hpp"
#include "engine/renderer/skybox.hpp"
#include "engine/scripting/script.hpp"

namespace bubble
{
// Object id shader to select entity from screen
constexpr string_view OBJECT_PICKING_SHADER = "./resources/shaders/object_picking"sv;
constexpr string_view WHITE_SHADER = "./resources/shaders/white"sv;
constexpr string_view PHONG_SHADER = "./resources/shaders/phong"sv;
constexpr string_view ONLY_DIFFUSE_SHADER = "./resources/shaders/only_diffuse"sv;

struct TextureData
{
    Scope<u8[]> mData;
    Texture2DSpecification mSpec;
    path mPath;
};

Ref<Script> LoadScript( const path& path );

TextureData OpenTexture( const path& path );
Ref<Texture2D> LoadTexture2D( const path& path );
Ref<Texture2D> LoadTexture2D( const TextureData& textureData );

Ref<Model> LoadModel( const path& path );
Ref<Model> LoadModel( const TextureData& path );

Ref<Shader> LoadShader( const path& path );
Ref<Skybox> LoadSkybox( const path& path );


struct Loader
{
    path mProjectRootPath;
    hash_map<path, Ref<Texture2D>> mTextures;
    hash_map<path, Ref<Model>> mModels;
    hash_map<path, Ref<Shader>> mShaders;
    hash_map<path, Ref<Skybox>> mSkyboxes;
    hash_map<path, Ref<Script>> mScripts;

    Ref<Script> LoadScript( const path& path );
    Ref<Texture2D> LoadTexture2D( const path& path );
    void LoadTextures2D( const vector<path>& paths );
    Ref<Shader> LoadShader( const path& path );
    Ref<Model> LoadModel( const path& path );
    void LoadModels( const vector<path>& paths );
    Ref<Skybox> LoadSkybox( const path& path );

    
    struct RelAbsPaths
    {
        path rel;
        path abs;
    };

    RelAbsPaths RelAbsFromProjectPath( const path& resPath ) const
    {
        // engine resource
        if ( resPath.string().starts_with( "." ) )
            return { resPath, resPath };

        auto relPath = resPath.is_relative() ? resPath : filesystem::relative( resPath, mProjectRootPath );
        auto absPath = resPath.is_absolute() ? resPath : mProjectRootPath / resPath;
        return RelAbsPaths{ .rel=relPath.generic_string(),
                            .abs=absPath.generic_string() };
    }
};

}