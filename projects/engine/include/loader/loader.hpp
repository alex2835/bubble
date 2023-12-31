#pragma once
#include <unordered_map>
#include <exception>
#include <tuple>
#include <format>
#include <string_view>
#include "log/log.hpp"
#include "utils/imexp.hpp"
#include "utils/pointers.hpp"
#include "utils/filesystem.hpp"
#include "renderer/texture.hpp"
#include "renderer/shader.hpp"
#include "renderer/model.hpp"
#include "renderer/skybox.hpp"

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

    std::unordered_map<std::string, Ref<Texture2D>> _mDefaultTextures;
    std::unordered_map<std::string, Ref<Model>>     _mDefaultModels;
    std::unordered_map<std::string, Ref<Shader>>    _mDefaultShaders;
    std::unordered_map<std::string, Ref<Skybox>>    _mDefaultSkyboxes;
    std::unordered_map<std::string, Ref<Texture2D>> _mDefaultSkyspheres;


    Ref<Shader> LoadShader( const std::path& path );
    Ref<Shader> LoadShader( const std::string_view name,
                            const std::string_view vertex,
                            const std::string_view fragment,
                            const std::string_view geometry = std::string() );

    Ref<Model> LoadModel( const std::path& path );

    //Ref<Texture2D> LoadAndCacheTexture2D( std::string path );
    Ref<Texture2D> LoadTexture2D( const std::path& path );
    std::tuple<Scope<uint8_t[]>, Texture2DSpecification> OpenRawImage( const std::path& path );

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