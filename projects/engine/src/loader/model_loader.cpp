#include "engine/utils/filesystem.hpp"
#include "engine/renderer/material.hpp"
#include "engine/loader/loader.hpp"
#include "assimp/Importer.hpp"
#include "assimp/Exporter.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

namespace bubble
{

//Ref<Model> Loader::LoadAndCacheModel( std::string path )
//{
//	if ( mModels.count( path ) )
//		return mModels[path];
//
//	auto model = LoadModel( path );
//	mModels.emplace( path, model );
//	return model;
//}

//void Loader::LoadSystemModel( std::string path )
//{
//	auto model = LoadModel( path );
//	mSystemModels.emplace( GetModelName( path ), model );
//}
//
//Ref<Model> Loader::GetSystemModel( std::string name )
//{
//	if ( !mSystemModels.count( name ) )
//		BUBBLE_ASSERT( false, "System model is not found: " + name );
//	return mSystemModels[name];
//}

Ref<Model> Loader::LoadModel( const std::path& path )
{
	auto model = CreateRef<Model>();

	Assimp::Exporter exporter;

	Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile( path.string(), 0 );
	if ( !scene || ( scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ) || !scene->mRootNode )
		throw std::runtime_error( "ERROR::ASSIMP\n" + std::string( importer.GetErrorString() ) );
	importer.ApplyPostProcessing( aiProcess_FlipUVs | aiProcessPreset_TargetRealtime_MaxQuality );

	model->mMeshes.reserve( scene->mNumMeshes );
	model->mRootNode = ProcessNode( *model, scene->mRootNode, scene, path );
	model->CreateBoundingBox();
	//model->mShader = GetSystemShader( "Phong shader" );
	return model;
}


Scope<MeshNode> Loader::ProcessNode( Model& model,
                                     const aiNode* node,
                                     const aiScene* scene,
                                     const std::path& path )
{
	Scope<MeshNode> mesh_node = CreateScope<MeshNode>( node->mName.C_Str() );

	for ( uint32_t i = 0; i < node->mNumMeshes; i++ )
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		model.mMeshes.emplace_back( ProcessMesh( mesh, scene, path ) );
		mesh_node->mMeshes.push_back( &model.mMeshes.back() );
	}

	for ( uint32_t i = 0; i < node->mNumChildren; i++ )
		mesh_node->mChildern.push_back( ProcessNode( model, node->mChildren[i], scene, path ) );

	return std::move( mesh_node );
}


Mesh Loader::ProcessMesh( const aiMesh* mesh,
                          const aiScene* scene,
                          const std::path& path )
{
	VertexData vertices;
	std::vector<uint32_t> indices;

	vertices.mPositions.resize( mesh->mNumVertices );
	vertices.mNormals.resize( mesh->mNumVertices );
	vertices.mTexCoords.resize( mesh->mNumVertices );
	indices.reserve( mesh->mNumFaces );

	if ( mesh->HasTangentsAndBitangents() )
	{
		vertices.mTangents.resize( mesh->mNumVertices );
		vertices.mBitangents.resize( mesh->mNumVertices );
	}

	// Texture coordinates
	if ( mesh->mTextureCoords[0] )
	{
		for ( uint32_t i = 0; i < mesh->mNumVertices; i++ )
		{
			vertices.mTexCoords[i].x = mesh->mTextureCoords[0][i].x;
			vertices.mTexCoords[i].y = mesh->mTextureCoords[0][i].y;
		}
	}

	// Faces
	for ( uint32_t i = 0; i < mesh->mNumFaces; i++ )
	{
		aiFace face = mesh->mFaces[i];
		for ( uint32_t j = 0; j < face.mNumIndices; j++ )
			indices.push_back( face.mIndices[j] );
	}

	memmove( vertices.mPositions.data(), mesh->mVertices, sizeof( glm::vec3 ) * vertices.mPositions.size() );
	memmove( vertices.mNormals.data(), mesh->mNormals, sizeof( glm::vec3 ) * vertices.mNormals.size() );
	memmove( vertices.mTangents.data(), mesh->mTangents, sizeof( glm::vec3 ) * vertices.mTangents.size() );
	memmove( vertices.mBitangents.data(), mesh->mBitangents, sizeof( glm::vec3 ) * vertices.mBitangents.size() );

	// Process materials
	aiMaterial* assimp_material = scene->mMaterials[mesh->mMaterialIndex];
	BasicMaterial material = LoadMaterialTextures( assimp_material, path );

	return Mesh( mesh->mName.C_Str(), std::move( material ), std::move( vertices ), std::move( indices ) );
}

BasicMaterial Loader::LoadMaterialTextures( const aiMaterial* mat, const std::path& path )
{
	const aiTextureType types[] = { aiTextureType_DIFFUSE , aiTextureType_SPECULAR, aiTextureType_HEIGHT, aiTextureType_NORMALS };

	// retrieve the directory path from filepath
	auto directory = path.parent_path();

	BasicMaterial material;
	for ( uint32_t i = 0; i < 3; i++ )
	{
		for ( uint32_t i = 0; i < 3; i++ )
		{
			for ( uint32_t j = 0; j < mat->GetTextureCount( types[i] ); j++ )
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

				case aiTextureType_HEIGHT:
					material.mNormalMap = LoadTexture2D( directory / str.C_Str() );
					break;

				default:
					LogError( "Model: {} | Does't use texture: {}", path.string(), str.C_Str());
				}
			}
		}
	}
	// Load material coefficients
	//aiColor4D diffuse;
	//ai_real specular[3];
	//ai_real ambient[3];
	//ai_real shininess;
	//
	//if ( AI_SUCCESS == aiGetMaterialColor( mat, AI_MATKEY_COLOR_DIFFUSE, &diffuse ) )
	//	material.mDiffuseColor = *(glm::vec4*)&diffuse;
	//
	//if ( AI_SUCCESS == aiGetMaterialFloat( mat, AI_MATKEY_COLOR_SPECULAR, specular ) )
	//	material.mSpecularCoef = *(float*)specular;
	//
	//if ( AI_SUCCESS == aiGetMaterialFloat( mat, AI_MATKEY_COLOR_AMBIENT, ambient ) )
	//	material.mAmbientCoef = *(float*)ambient;
	//
	//if ( AI_SUCCESS == aiGetMaterialFloat( mat, AI_MATKEY_SHININESS, &shininess ) )
	//	if ( shininess )
	//		material.mShininess = static_cast<int>( shininess );

	// If no textures create 1x1 white one
	//Ref<Texture2D> white_plug = LoadTexture2DSingleColor( "white plug", glm::vec4( 1.0f ) );

	//if ( !material.mDiffuseMap )
	//	material.mDiffuseMap = white_plug;
	//
	//if ( !material.mSpecularMap )
	//	material.mSpecularMap = white_plug;

	return material;
}

}