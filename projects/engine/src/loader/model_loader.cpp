#include "engine/utils/filesystem.hpp"
#include "engine/renderer/material.hpp"
#include "engine/loader/loader.hpp"
#include "assimp/Importer.hpp"
#include "assimp/Exporter.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "thread_pool.hpp"
#include "fixed_size_packaged_task.hpp"

namespace bubble
{
constexpr array<aiTextureType, 4> cTextureTypes{ aiTextureType_DIFFUSE ,
                                                 aiTextureType_SPECULAR,
                                                 aiTextureType_HEIGHT,
                                                 aiTextureType_NORMALS };

struct ModelData
{
    Scope<Assimp::Importer> mImporter;
    map<path, TextureData> mTexturesData;
    path mPath;
};


map<path, TextureData> LoadModelTexturesData( const path& modelDirectory,
                                              const aiScene* scene )
{
    map<path, TextureData> texturesData;

    ThreadPool threadPool;
    std::vector<FixedSizePackagedTask<pair<path, TextureData>()>> texturesDataTasks;

    for ( u32 materialIndex = 0; materialIndex < scene->mNumMaterials; materialIndex++ )
    {
        auto material = scene->mMaterials[materialIndex];
        for ( u32 textureTypeIndex = 0; textureTypeIndex < cTextureTypes.size(); textureTypeIndex++ )
        {
            auto textureType = cTextureTypes[textureTypeIndex];
            auto texturesCount = material->GetTextureCount( textureType );
            for ( u32 textureIndex = 0; textureIndex < texturesCount; textureIndex++ )
            {
                aiString textureName;
                material->GetTexture( textureType, textureIndex, &textureName );
                auto texturePath = modelDirectory / textureName.C_Str();
                texturesDataTasks.emplace_back( [texturePath]()
                {
                    auto textureData = OpenTexture( texturePath );
                    return std::make_pair( texturePath, std::move( textureData ) );
                } );
            }
        }
    }
    threadPool.AddTasks( texturesDataTasks );

    for ( auto& task : texturesDataTasks )
    {
        auto [texturePath, textureData] = task.get();
        texturesData.emplace( texturePath, std::move( textureData ) );
    }
    return texturesData;
}


ModelData OpenModel( const path& modelPath )
{
    auto importer = CreateScope<Assimp::Importer>();
    const aiScene* scene = importer->ReadFile( modelPath.string(), 0 );
    if ( !scene || ( scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ) || !scene->mRootNode )
        throw std::runtime_error( "ERROR::ASSIMP\n" + string( importer->GetErrorString() ) );
    importer->ApplyPostProcessing( aiProcess_FlipUVs | aiProcessPreset_TargetRealtime_MaxQuality );

    auto texturesData = LoadModelTexturesData( modelPath.parent_path(), importer->GetScene() );
    return ModelData{ std::move( importer ), std::move( texturesData ), modelPath };
}



BasicMaterial LoadMaterialTextures( const aiMaterial* mat, const path& modelDirectory )
{
    BasicMaterial material;
    for ( u32 i = 0; i < cTextureTypes.size(); i++ )
    {
        auto texturesCount = mat->GetTextureCount( cTextureTypes[i] );
        for ( u32 j = 0; j < texturesCount; j++ )
        {
            aiString str;
            mat->GetTexture( cTextureTypes[i], j, &str );

            switch ( cTextureTypes[i] )
            {
            case aiTextureType_DIFFUSE:
                material.mDiffuseMap = LoadTexture2D( modelDirectory / str.C_Str() );
                break;
            case aiTextureType_SPECULAR:
                material.mSpecularMap = LoadTexture2D( modelDirectory / str.C_Str() );
                break;
            case aiTextureType_NORMALS:
                material.mNormalMap = LoadTexture2D( modelDirectory / str.C_Str() );
                break;
                //case aiTextureType_HEIGHT:
                //    material.mNormalMap = LoadTexture2D( directory / str.C_Str() );
                //    break;
            default:
                LogWarning( "Model: {}. Doesn't use texture: {}", modelDirectory.string(), str.C_Str() );
            }
        }
    }
    // Load material basic variables	
    aiColor4D diffuse;
    if ( AI_SUCCESS == aiGetMaterialColor( mat, AI_MATKEY_COLOR_DIFFUSE, &diffuse ) )
        material.mDiffuseColor = vec4( diffuse.r, diffuse.g, diffuse.b, diffuse.a );

    aiColor4D specular;
    if ( AI_SUCCESS == aiGetMaterialColor( mat, AI_MATKEY_COLOR_SPECULAR, &specular ) )
        material.mSpecular = vec4( specular.r, specular.g, specular.b, specular.a );

    aiColor4D ambient;
    if ( AI_SUCCESS == aiGetMaterialColor( mat, AI_MATKEY_COLOR_AMBIENT, &ambient ) )
        material.mAmbient = vec4( ambient.r, ambient.g, ambient.b, ambient.a );

    aiColor4D emission;
    if ( AI_SUCCESS == aiGetMaterialColor( mat, AI_MATKEY_COLOR_EMISSIVE, &emission ) )
        material.mEmission = vec4( emission.r, emission.g, emission.b, emission.a );
    
    ai_real shininess;
    if ( AI_SUCCESS == aiGetMaterialFloat( mat, AI_MATKEY_SHININESS, &shininess ) )
        material.mShininess = static_cast<i32>( shininess );

    ai_real strength;
    if ( AI_SUCCESS == aiGetMaterialFloat( mat, AI_MATKEY_SHININESS_STRENGTH, &strength ) )
        material.mShininessStrength = strength;

    return material;
}


Mesh ProcessMesh( const aiMesh* mesh,
                  const aiScene* scene,
                  const path& modelPath )
{
    VertexData vertices;

    vertices.mPositions.resize( mesh->mNumVertices );
    vertices.mNormals.resize( mesh->mNumVertices );
    memmove( vertices.mPositions.data(), mesh->mVertices, sizeof( vec3 ) * vertices.mPositions.size() );
    memmove( vertices.mNormals.data(), mesh->mNormals, sizeof( vec3 ) * vertices.mNormals.size() );

    // Faces
    vector<u32> indices;
    indices.reserve( mesh->mNumFaces );
    for ( u32 i = 0; i < mesh->mNumFaces; i++ )
    {
        aiFace face = mesh->mFaces[i];
        for ( u32 j = 0; j < face.mNumIndices; j++ )
            indices.push_back( face.mIndices[j] );
    }

    // Texture coordinates
    vertices.mTexCoords.resize( mesh->mNumVertices );
    if ( mesh->mTextureCoords[0] )
    {
        for ( u32 i = 0; i < mesh->mNumVertices; i++ )
        {
            vertices.mTexCoords[i].x = mesh->mTextureCoords[0][i].x;
            vertices.mTexCoords[i].y = mesh->mTextureCoords[0][i].y;
        }
    }

    // Tangents and Bitangents
    if ( mesh->HasTangentsAndBitangents() )
    {
        vertices.mTangents.resize( mesh->mNumVertices );
        vertices.mBitangents.resize( mesh->mNumVertices );
        memmove( vertices.mTangents.data(), mesh->mTangents, sizeof( vec3 ) * vertices.mTangents.size() );
        memmove( vertices.mBitangents.data(), mesh->mBitangents, sizeof( vec3 ) * vertices.mBitangents.size() );
    }

    // Material
    aiMaterial* assimp_material = scene->mMaterials[mesh->mMaterialIndex];
    BasicMaterial material = LoadMaterialTextures( assimp_material, modelPath.parent_path() );
    
    return Mesh( mesh->mName.C_Str(),
                 std::move( material ),
                 std::move( vertices ),
                 std::move( indices ) );
}


Scope<MeshTreeViewNode> ProcessNode( Model& model,
                                     const aiNode* node,
                                     const aiScene* scene,
                                     const path& modelPath )
{
    auto mesh_node = CreateScope<MeshTreeViewNode>( node->mName.C_Str() );

    for ( u32 i = 0; i < node->mNumMeshes; i++ )
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        model.mMeshes.push_back( ProcessMesh( mesh, scene, modelPath ) );
        mesh_node->mMeshes.push_back( &model.mMeshes.back() );
    }
    for ( u32 i = 0; i < node->mNumChildren; i++ )
        mesh_node->mChildren.push_back( ProcessNode( model, node->mChildren[i], scene, modelPath ) );

    return std::move( mesh_node );
}


Ref<Model> LoadModel( const ModelData& modelData )
{
    auto scene = modelData.mImporter->GetScene();

    auto model = CreateRef<Model>();
    model->mName = modelData.mPath.stem().string();
    model->mPath = modelData.mPath;
    model->mMeshes.reserve( scene->mNumMeshes );
    model->mRootMeshTreeView = ProcessNode( *model, scene->mRootNode, scene, modelData.mPath );
    //model->CreateBoundingBox();
    return model;
}


Ref<Model> LoadModel( const path& path )
{
    auto modelData = OpenModel( path );
    return LoadModel( modelData );
}


Ref<Model> Loader::LoadModel( const path& modelPath )
{
    auto[relPath,absPath] = RelAbsFromProjectPath( modelPath );

    auto iter = mModels.find( relPath );
    if ( iter != mModels.end() )
        return iter->second;

	auto model = bubble::LoadModel( absPath );
	mModels.emplace( relPath, model );
    return model;
}


void Loader::LoadModels( const vector<path>& modelsPaths )
{
    ThreadPool threadPool;
    std::vector<FixedSizePackagedTask<pair<path,ModelData>(), 128>> modelDataTasks;

    for ( const path& modelPath : modelsPaths )
    {
        auto [relPath, absPath] = RelAbsFromProjectPath( modelPath );

        if ( mModels.contains( modelPath ) )
            continue;

        modelDataTasks.emplace_back( [=]()
        {
            return std::make_pair( relPath, OpenModel( absPath ) );
        } );
    }
    threadPool.AddTasks( modelDataTasks );

    for ( auto& task : modelDataTasks )
    {
        auto [relModelPath, modelData] = task.get();
        mModels[relModelPath] = bubble::LoadModel( modelData );
    }
}

}