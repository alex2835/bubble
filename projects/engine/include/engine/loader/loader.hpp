#pragma once
#include <unordered_map>
#include <exception>
#include <tuple>
#include <format>
#include <string_view>
#include "engine/log/log.hpp"
#include "engine/utils/imexp.hpp"
#include "engine/utils/types.hpp"
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
    std::unordered_map<string, Ref<Texture2D>> mTextures;
    std::unordered_map<string, Ref<Model>>     mModels;
    std::unordered_map<string, Ref<Shader>>    mShaders;
    std::unordered_map<string, Ref<Skybox>>    mSkyboxes;
    std::unordered_map<string, Ref<Texture2D>> mSkyspheres;

    std::unordered_map<string, Ref<Texture2D>> mDefaultTextures;
    std::unordered_map<string, Ref<Model>>     mDefaultModels;
    std::unordered_map<string, Ref<Shader>>    mDefaultShaders;
    std::unordered_map<string, Ref<Skybox>>    mDefaultSkyboxes;
    std::unordered_map<string, Ref<Texture2D>> mDefaultSkyspheres;


    Ref<Shader> LoadShader( const path& path );
    Ref<Shader> LoadShader( const std::string_view name,
                            const std::string_view vertex,
                            const std::string_view fragment,
                            const std::string_view geometry = string() );

    Ref<Model> LoadModel( const path& path );

    //Ref<Texture2D> LoadAndCacheTexture2D( string path );
    Ref<Texture2D> LoadTexture2D( const path& path );
    //std::tuple<Scope<uint8_t[]>, Texture2DSpecification> OpenRawImage( const path& path );

private:
    Scope<MeshNode> ProcessNode( Model& model, const aiNode* node, const aiScene* scene, const path& path );
    Mesh ProcessMesh( const aiMesh* mesh, const aiScene* scene, const path& path );
    BasicMaterial LoadMaterialTextures( const aiMaterial* mat, const path& path );

    void ParseShaders( const path& path,
                       string& vertex,
                       string& fragment,
                       string& geometry );

    void CompileShaders( Shader& shader,
                         const std::string_view vertex_source,
                         const std::string_view fragment_source,
                         const std::string_view geometry_source );
};

}