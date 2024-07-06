#pragma once
#include "engine/utils/imexp.hpp"
#include "engine/log/log.hpp"
#include "engine/utils/types.hpp"
#include "engine/utils/filesystem.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/shader.hpp"
#include "engine/renderer/model.hpp"
#include "engine/renderer/skybox.hpp"
#include <mutex>

struct aiMesh;
struct aiNode;
struct aiScene;
struct aiMaterial;

namespace bubble
{
constexpr string_view OBJECT_PICKING_SHADER = "./resources/shaders/object_picking"sv;
constexpr string_view WHITE_SHADER = "./resources/shaders/white"sv;
constexpr string_view PHONG_SHADER = "./resources/shaders/phong"sv;
constexpr string_view ONLY_DEFUSE_SHADER = "./resources/shaders/only_defuse"sv;
//constexpr string_view PHONG_SHADER = "C:/Users/sa007/Desktop/projects/Bubble/resources/shaders/phong"sv;

BUBBLE_ENGINE_EXPORT std::pair<Scope<u8[]>, Texture2DSpecification> OpenImage( const path& path );
BUBBLE_ENGINE_EXPORT Ref<Texture2D> LoadTexture2D( const path& path );
BUBBLE_ENGINE_EXPORT Ref<Shader> LoadShader( const path& path );
BUBBLE_ENGINE_EXPORT Ref<Model> LoadModel( const path& path );
BUBBLE_ENGINE_EXPORT Ref<Skybox> LoadSkybox( const path& path );


struct BUBBLE_ENGINE_EXPORT Loader
{
    hash_map<path, Ref<Texture2D>> mTextures;
    hash_map<path, Ref<Model>> mModels;
    hash_map<path, Ref<Shader>> mShaders;
    hash_map<path, Ref<Skybox>> mSkyboxes;
    
    Ref<Texture2D> LoadTexture2D( const path& path );
    Ref<Shader> LoadShader( const path& path );
    Ref<Model> LoadModel( const path& path );
    Ref<Skybox> LoadSkybox( const path& path );
};

}