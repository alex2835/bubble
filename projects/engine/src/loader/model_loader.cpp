#include "engine/utils/filesystem.hpp"
#include "engine/renderer/material.hpp"
#include "engine/loader/loader.hpp"
#include "assimp/Importer.hpp"
#include "assimp/Exporter.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

namespace bubble
{

//Ref<Model> Loader::LoadAndCacheModel( string path )
//{
//	if ( mModels.count( path ) )
//		return mModels[path];
//
//	auto model = LoadModel( path );
//	mModels.emplace( path, model );
//	return model;
//}

//void Loader::LoadSystemModel( string path )
//{
//	auto model = LoadModel( path );
//	mSystemModels.emplace( GetModelName( path ), model );
//}
//
//Ref<Model> Loader::GetSystemModel( string name )
//{
//	if ( !mSystemModels.count( name ) )
//		BUBBLE_ASSERT( false, "System model is not found: " + name );
//	return mSystemModels[name];
//}

Ref<Model> Loader::LoadModel( const path& path )
{
	auto model = CreateRef<Model>();

	Assimp::Exporter exporter;

	Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile( path.string(), 0 );
	if ( !scene || ( scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ) || !scene->mRootNode )
		throw std::runtime_error( "ERROR::ASSIMP\n" + string( importer.GetErrorString() ) );
	importer.ApplyPostProcessing( aiProcess_FlipUVs | aiProcessPreset_TargetRealtime_MaxQuality );

	auto fullname = path.filename().string();
	model->mName = fullname.substr( 0, fullname.find( '.' ) );
	model->mMeshes.reserve( scene->mNumMeshes );
	model->mRootMeshNode = ProcessNode( *model, scene->mRootNode, scene, path );
	//model->CreateBoundingBox();
	//model->mShader = GetSystemShader( "Phong shader" );
	return model;
}


Scope<MeshNode> Loader::ProcessNode( Model& model,
                                     const aiNode* node,
                                     const aiScene* scene,
                                     const path& path )
{
	Scope<MeshNode> mesh_node = CreateScope<MeshNode>( node->mName.C_Str() );

	for ( u32 i = 0; i < node->mNumMeshes; i++ )
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		model.mMeshes.emplace_back( ProcessMesh( mesh, scene, path ) );
		mesh_node->mMeshes.push_back( &model.mMeshes.back() );
	}

	for ( u32 i = 0; i < node->mNumChildren; i++ )
		mesh_node->mChildern.push_back( ProcessNode( model, node->mChildren[i], scene, path ) );

	return std::move( mesh_node );
}


Mesh Loader::ProcessMesh( const aiMesh* mesh,
                          const aiScene* scene,
                          const path& path )
{
	VertexData vertices;

	vertices.mPositions.resize( mesh->mNumVertices );
	vertices.mNormals.resize( mesh->mNumVertices );
    memmove( vertices.mPositions.data(), mesh->mVertices, sizeof( vec3 ) * vertices.mPositions.size() );
    memmove( vertices.mNormals.data(), mesh->mNormals, sizeof( vec3 ) * vertices.mNormals.size() );

	if ( mesh->HasTangentsAndBitangents() )
	{
		vertices.mTangents.resize( mesh->mNumVertices );
		vertices.mBitangents.resize( mesh->mNumVertices );
        memmove( vertices.mTangents.data(), mesh->mTangents, sizeof( vec3 ) * vertices.mTangents.size() );
        memmove( vertices.mBitangents.data(), mesh->mBitangents, sizeof( vec3 ) * vertices.mBitangents.size() );
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

	// Faces
    vector<u32> indices;
    indices.reserve( mesh->mNumFaces );
	for ( u32 i = 0; i < mesh->mNumFaces; i++ )
	{
		aiFace face = mesh->mFaces[i];
		for ( u32 j = 0; j < face.mNumIndices; j++ )
			indices.push_back( face.mIndices[j] );
	}

	// Material
	aiMaterial* assimp_material = scene->mMaterials[mesh->mMaterialIndex];
	BasicMaterial material = LoadMaterialTextures( assimp_material, path );

	return Mesh( mesh->mName.C_Str(), 
				 std::move( material ),
				 std::move( vertices ),
				 std::move( indices ) );
}

BasicMaterial Loader::LoadMaterialTextures( const aiMaterial* mat, const path& path )
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
                LogError( "Model: {}. Does't use texture: {}", path.string(), str.C_Str() );
            }
        }
    }
	// Load material basic variables
	aiColor4D diffuse;
	ai_real specular[3];
	ai_real ambient[3];
	ai_real shininess;
	
	if ( AI_SUCCESS == aiGetMaterialColor( mat, AI_MATKEY_COLOR_DIFFUSE, &diffuse ) )
		material.mDiffuseColor = vec4( diffuse.r, diffuse.g, diffuse.b, diffuse.a );
	
	if ( AI_SUCCESS == aiGetMaterialFloat( mat, AI_MATKEY_COLOR_SPECULAR, specular ) )
		material.mSpecularCoef = vec3( specular[0], specular[1], specular[2] );
	
	if ( AI_SUCCESS == aiGetMaterialFloat( mat, AI_MATKEY_COLOR_AMBIENT, ambient ) )
		material.mAmbientCoef = vec3( ambient[0], ambient[1], ambient[2] );
	
	if ( AI_SUCCESS == aiGetMaterialFloat( mat, AI_MATKEY_SHININESS, &shininess ) )
		if ( shininess )
			material.mShininess = static_cast<i32>( shininess );

	return material;
}

}