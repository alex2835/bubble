#pragma once
#include "engine/log/log.hpp"
#include "engine/types/string.hpp"
#include "engine/types/pointer.hpp"
#include "engine/types/map.hpp"
#include "engine/types/array.hpp"
#include "engine/types/utility.hpp"
#include "engine/utils/filesystem.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/shader.hpp"
#include "engine/renderer/model.hpp"
#include "engine/renderer/skybox.hpp"
#include "engine/scripting/script.hpp"
#include "engine/loader/shader_module_loader.hpp"

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
// shader paths
constexpr string_view ENTITY_PICKING_SHADER = "./resources/shaders/object_picking"sv; // Object id shader to select entity from screen
constexpr string_view ENTITY_PICKING_BILLBOARD_SHADER = "./resources/shaders/object_picking_billboard"sv;
constexpr string_view WHITE_SHADER = "./resources/shaders/white"sv;
constexpr string_view PHONG_SHADER = "./resources/shaders/phong"sv;
constexpr string_view ONLY_DIFFUSE_SHADER = "./resources/shaders/only_diffuse"sv;
constexpr string_view BILBOARD_SHADER = "./resources/shaders/billboard"sv;
// Used when a shader supplies a fragment stage and no vertex stage of its own.
// Most shaders differ only in how they shade a fragment, and needing a copy of
// this file per shader is what makes every new shader start as a duplicate.
constexpr string_view DEFAULT_VERTEX_SHADER = "./resources/shaders/phong.vert"sv;


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

    // Values are unique across every Loader in the process, never reused and
    // never reset, so replacing the whole Loader on a project switch always
    // reads as a change - even if the new project happens to load the same
    // number of resources before anyone looks.
    static u64 NextResourcesGeneration();


public:
    path mProjectRootDir;
    hash_map<path, Ref<Texture2D>> mTextures;
    hash_map<path, Ref<Model>> mModels;
    hash_map<path, Ref<Shader>> mShaders;
    hash_map<path, Ref<Skybox>> mSkyboxes;
    hash_map<path, Ref<Script>> mScripts;

    // Stamped whenever mShaders or mScripts gains an entry, so an observer can
    // tell in one comparison whether its view of them is out of date. Erasing
    // from those maps directly does not go through the Loader, and so does not
    // stamp it.
    u64 mResourcesGeneration = NextResourcesGeneration();
};

}