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

namespace Assimp { class Importer; }

namespace bubble
{
// error resources
constexpr string_view ERROR_MODEL = "./resources/models/error.obj"sv;
constexpr string_view ERROR_TEXTURE = "./resources/images/utils/error.jpg"sv;
// scene icons textures
constexpr string_view SCENE_CAMERA_TEXTURE = "./resources/images/scene/camera.png"sv;
constexpr string_view SCENE_POINT_LIGHT_TEXTURE = "./resources/images/scene/pointlight.png"sv;
constexpr string_view SCENE_SPOT_LIGHT_TEXTURE = "./resources/images/scene/spotlight.png"sv;
constexpr string_view SCENE_DIR_LIGHT_TEXTURE = "./resources/images/scene/dirlight.png"sv;
// shader modules search path
constexpr string_view SHADER_MODULES_SEARCH_PATH = "./resources/shaders/modules"sv;
// shader paths
constexpr string_view ENTITY_PICKING_SHADER = "./resources/shaders/object_picking"sv; // Object id shader to select entity from screen
constexpr string_view ENTITY_PICKING_BILLBOARD_SHADER = "./resources/shaders/object_picking_billboard"sv;
constexpr string_view WHITE_SHADER = "./resources/shaders/white"sv;
constexpr string_view PHONG_SHADER = "./resources/shaders/phong"sv;
constexpr string_view ONLY_DIFFUSE_SHADER = "./resources/shaders/only_diffuse"sv;
constexpr string_view BILBOARD_SHADER = "./resources/shaders/billboard"sv;


struct TextureData
{
    Scope<u8[]> mData;
    Texture2DSpecification mSpec;
    path mPath;
};

struct ModelData
{
    Scope<Assimp::Importer> mImporter;
    map<path, TextureData> mTexturesData;
    path mPath;
};

std::optional<TextureData> OpenTexture( const path& path );
Ref<Texture2D> LoadTexture2D( const path& path );
Ref<Texture2D> LoadTexture2D( const TextureData& textureData );

Ref<Model> LoadModel( const path& path );
Ref<Model> LoadModel( const ModelData& modelData );

Ref<Shader> LoadShader( const path& path );
Ref<Skybox> LoadSkybox( const path& path );

Ref<Script> LoadScript( const path& path );



struct Loader
{
    Loader() = default;
    Ref<Script> LoadScript( const path& path );
    Ref<Texture2D> LoadTexture2D( const path& path );
    void LoadTextures2D( const vector<path>& paths );
    Ref<Shader> LoadShader( path path );
    Ref<Model> LoadModel( const path& path );
    void LoadModels( const vector<path>& paths );
    Ref<Skybox> LoadSkybox( const path& path );

    
    struct ProjectPath
    {
        path rel;
        path abs;
    };
    ProjectPath RelAbsFromProjectPath( const path& resourcePath ) const;


public:
    path mProjectRootPath;
    hash_map<path, Ref<Texture2D>> mTextures;
    hash_map<path, Ref<Model>> mModels;
    hash_map<path, Ref<Shader>> mShaders;
    hash_map<path, Ref<Skybox>> mSkyboxes;
    hash_map<path, Ref<Script>> mScripts;
};

}