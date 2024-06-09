#include "engine/utils/filesystem.hpp"
#include "engine/renderer/material.hpp"
#include "engine/loader/loader.hpp"
#include "assimp/Importer.hpp"
#include "assimp/Exporter.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

namespace bubble
{

BasicMaterial LoadMaterialTextures( const aiMaterial* mat, const path& path )
{
    constexpr auto textureTypes = 4;
    const aiTextureType types[textureTypes] = { aiTextureType_DIFFUSE ,
                                                aiTextureType_SPECULAR,
                                                aiTextureType_HEIGHT,
                                                aiTextureType_NORMALS };
    BasicMaterial material;
    auto directory = path.parent_path();
    for ( u32 i = 0; i < textureTypes; i++ )
    {
        auto texturesCount = mat->GetTextureCount( types[i] );
        for ( u32 j = 0; j < texturesCount; j++ )
        {
            aiString str;
            mat->GetTexture( types[i], j, &str );

            switch ( types[i] )
            {
            case aiTextureType_DIFFUSE:
                material.mDiffuseMap = LoadTexture2D( directory / str.C_Str() );
                break;
            case aiTextureType_SPECULAR:
                material.mSpecularMap = LoadTexture2D( directory / str.C_Str() );
                break;
            case aiTextureType_NORMALS:
                material.mNormalMap = LoadTexture2D( directory / str.C_Str() );
                break;
                //case aiTextureType_HEIGHT:
                //    material.mNormalMap = LoadTexture2D( directory / str.C_Str() );
                //    break;
            default:
                LogWarning( "Model: {}. Doesn't use texture: {}", path.string(), str.C_Str() );
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
                  const path& path )
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
    BasicMaterial material = LoadMaterialTextures( assimp_material, path );

    return Mesh( mesh->mName.C_Str(),
                 std::move( material ),
                 std::move( vertices ),
                 std::move( indices ) );
}


Scope<MeshTreeViewNode> ProcessNode( Model& model,
                                     const aiNode* node,
                                     const aiScene* scene,
                                     const path& path )
{
    auto mesh_node = CreateScope<MeshTreeViewNode>( node->mName.C_Str() );

    for ( u32 i = 0; i < node->mNumMeshes; i++ )
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        model.mMeshes.push_back( ProcessMesh( mesh, scene, path ) );
        mesh_node->mMeshes.push_back( &model.mMeshes.back() );
    }
    for ( u32 i = 0; i < node->mNumChildren; i++ )
        mesh_node->mChildren.push_back( ProcessNode( model, node->mChildren[i], scene, path ) );

    return std::move( mesh_node );
}



Ref<Model> LoadModel( const path& path )
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile( path.string(), 0 );
    if ( !scene || ( scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ) || !scene->mRootNode )
        throw std::runtime_error( "ERROR::ASSIMP\n" + string( importer.GetErrorString() ) );
    importer.ApplyPostProcessing( aiProcess_FlipUVs | aiProcessPreset_TargetRealtime_MaxQuality );

	auto model = CreateRef<Model>();
    model->mName = path.stem().string();
	model->mPath = path;
    model->mMeshes.reserve( scene->mNumMeshes );
    model->mRootMeshTreeView = ProcessNode( *model, scene->mRootNode, scene, path );
    //model->CreateBoundingBox();
	return model;
}


Ref<Model> Loader::LoadModel( const path& path )
{
	auto iter = mModels.find( path );
    if ( iter != mModels.end() )
        return iter->second;

	auto model = bubble::LoadModel( path );
    //model->mShader = LoadShader( PHONG_SHADER ); // default shader
	mModels.emplace( path, model );
    return model;
}

}