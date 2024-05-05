#pragma once
#include "engine/utils/imexp.hpp"
#include "engine/log/log.hpp"
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
    hash_map<path, Ref<Texture2D>> mTextures;
    hash_map<path, Ref<Model>>     mModels;
    hash_map<path, Ref<Shader>>    mShaders;
    hash_map<path, Ref<Skybox>>    mSkyboxes;

    static std::pair<Scope<u8[]>, Texture2DSpecification> OpenImage(const path& path);
    static Ref<Texture2D> JustLoadTexture2D( const path& path );
    Ref<Texture2D> LoadTexture2D( const path& path );

    static Ref<Shader> JustLoadShader( string name, string_view vertex, string_view fragment, string_view geometry = {} );
    static Ref<Shader> JustLoadShader( const path& path );
    Ref<Shader> LoadShader( const path& path );

    static Ref<Model> JustLoadModel( const path& path );
    Ref<Model> LoadModel( const path& path );

    static Ref<Skybox> JustLoadSkybox( const path& path );
    Ref<Skybox> LoadSkybox( const path& path );

private:
    // Shader
    static void ParseShaders( const path& path,
                              string& vertex,
                              string& fragment,
                              string& geometry );

    static void CompileShaders( Shader& shader,
                                string_view vertex_source,
                                string_view fragment_source,
                                string_view geometry_source );

    // Model
    static Scope<MeshTreeViewNode> ProcessNode( Model& model, const aiNode* node, const aiScene* scene, const path& path );
    static Mesh ProcessMesh( const aiMesh* mesh, const aiScene* scene, const path& path );
    static BasicMaterial LoadMaterialTextures( const aiMaterial* mat, const path& path );
};

}