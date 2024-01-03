#pragma once
#include <unordered_map>
#include <exception>
#include <tuple>
#include <format>
#include <string_view>
#include "engine/log/log.hpp"
#include "engine/utils/imexp.hpp"
#include "engine/utils/pointers.hpp"
#include "engine/utils/filesystem.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/shader.hpp"
#include "engine/renderer/model.hpp"
#include "engine/renderer/skybox.hpp"

struct aiMesh;
struct aiNode;
struct aiScene;
struct aiMaterial;

namespace bubble
{
struct BUBBLE_ENGINE_EXPORT Loader
{
    std::unordered_map<std::string, Ref<Texture2D>> mTextures;
    std::unordered_map<std::string, Ref<Model>>     mModels;
    std::unordered_map<std::string, Ref<Shader>>    mShaders;
    std::unordered_map<std::string, Ref<Skybox>>    mSkyboxes;
    std::unordered_map<std::string, Ref<Texture2D>> mSkyspheres;

    std::unordered_map<std::string, Ref<Texture2D>> mDefaultTextures;
    std::unordered_map<std::string, Ref<Model>>     mDefaultModels;
    std::unordered_map<std::string, Ref<Shader>>    mDefaultShaders;
    std::unordered_map<std::string, Ref<Skybox>>    mDefaultSkyboxes;
    std::unordered_map<std::string, Ref<Texture2D>> mDefaultSkyspheres;


    Ref<Shader> LoadShader( const std::path& path );
    Ref<Shader> LoadShader( const std::string_view name,
                            const std::string_view vertex,
                            const std::string_view fragment,
                            const std::string_view geometry = std::string() );

    Ref<Model> LoadModel( const std::path& path );

    //Ref<Texture2D> LoadAndCacheTexture2D( std::string path );
    Ref<Texture2D> LoadTexture2D( const std::path& path );
    //std::tuple<Scope<uint8_t[]>, Texture2DSpecification> OpenRawImage( const std::path& path );

private:
    Scope<MeshNode> ProcessNode( Model& model, const aiNode* node, const aiScene* scene, const std::path& path );
    Mesh ProcessMesh( const aiMesh* mesh, const aiScene* scene, const std::path& path );
    BasicMaterial LoadMaterialTextures( const aiMaterial* mat, const std::path& path );

    void ParseShaders( const std::path& path,
                       std::string& vertex,
                       std::string& fragment,
                       std::string& geometry );

    void CompileShaders( Shader& shader,
                         const std::string_view vertex_source,
                         const std::string_view fragment_source,
                         const std::string_view geometry_source );
};

}