#include "engine/pch/pch.hpp"
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
// The material slots the engine has, in the order they are looked up.
//
// HEIGHT is here because of OBJ: assimp reports map_bump / bump as HEIGHT and
// only norm as NORMALS, and what OBJ files call a bump map is nearly always a
// tangent space normal map. It stands in for the normal map when the material
// has no NORMALS entry, which is why NORMALS is listed first.
constexpr array<aiTextureType, 4> cTextureTypes{ aiTextureType_DIFFUSE,
                                                 aiTextureType_SPECULAR,
                                                 aiTextureType_NORMALS,
                                                 aiTextureType_HEIGHT };


// mHeight == 0 means the texture is a still encoded file (png, jpg) of mWidth
// bytes; otherwise it is raw texels, which assimp stores as BGRA.
std::optional<TextureData> OpenEmbeddedTexture( const aiTexture* texture, const path& name )
{
    if ( texture->mHeight == 0 )
        return OpenTexture( reinterpret_cast<const u8*>( texture->pcData ), texture->mWidth, name );

    const u64 texelCount = u64( texture->mWidth ) * texture->mHeight;
    Scope<u8[]> data( new u8[texelCount * 4] );
    for ( u64 i = 0; i < texelCount; i++ )
    {
        const aiTexel& texel = texture->pcData[i];
        data[i * 4 + 0] = texel.r;
        data[i * 4 + 1] = texel.g;
        data[i * 4 + 2] = texel.b;
        data[i * 4 + 3] = texel.a;
    }
    auto spec = Texture2DSpecification::CreateRGBA8( { (i32)texture->mWidth, (i32)texture->mHeight } );
    spec.SetTextureSpecChanels( 4 );
    return TextureData{ std::move( data ), spec, name };
}


map<path, TextureData> LoadModelTexturesData( const path& modelDirectory,
                                              const aiScene* scene )
{
    map<path, TextureData> texturesData;

    ThreadPool threadPool;
    std::vector<FixedSizePackagedTask<pair<path, std::optional<TextureData>>()>> texturesDataTasks;

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

                // A glTF binary carries its images inside the file; the
                // material names them "*N". The path is still the map key
                // LoadMaterial looks up, it just never touches the disk.
                if ( const aiTexture* embedded = scene->GetEmbeddedTexture( textureName.C_Str() ) )
                {
                    texturesDataTasks.emplace_back( [texturePath, embedded]()
                    {
                        return std::make_pair( texturePath, OpenEmbeddedTexture( embedded, texturePath ) );
                    } );
                    continue;
                }

                texturesDataTasks.emplace_back( [texturePath]()
                {
                    return std::make_pair( texturePath, OpenTexture( texturePath ) );
                } );
            }
        }
    }
    threadPool.AddTasks( texturesDataTasks );

    for ( auto& task : texturesDataTasks )
    {
        auto [texturePath, textureData] = task.get();
        if ( not textureData )
        {
            LogError( "Failed to load model texture: {}", texturePath.string() );
            continue;
        }
        texturesData.emplace( texturePath, std::move( *textureData ) );
    }
    return texturesData;
}


std::optional<ModelData> OpenModel( const path& modelPath )
{
    auto importer = CreateScope<Assimp::Importer>();
    // FBX pivots would otherwise become "$AssimpFbx$_..." helper nodes between
    // every bone and its parent - and joints of the skeleton.
    importer->SetPropertyBool( AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false );
    const aiScene* scene = importer->ReadFile( modelPath.string(), aiProcess_GenSmoothNormals );
    if ( !scene || ( scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ) || !scene->mRootNode )
    {
        LogError("ERROR::ASSIMP\n" + string( importer->GetErrorString() ) );
        return std::nullopt;
    }
    importer->ApplyPostProcessing( aiProcess_FlipUVs | aiProcessPreset_TargetRealtime_MaxQuality );

    auto texturesData = LoadModelTexturesData( modelPath.parent_path(), importer->GetScene() );
    auto skeleton = ImportSkeleton( importer->GetScene(), modelPath );
    return ModelData{ std::move( importer ), std::move( texturesData ), std::move( skeleton ), modelPath };
}



BasicMaterial LoadMaterial( const aiMaterial* mat,
                            const ModelData& modelData,
                            const TextureUploader& uploadTexture )
{
    const path modelDirectory = modelData.mPath.parent_path();

    // The files were decoded by OpenModel; here they are only uploaded. One
    // that failed to decode is already logged and absent from the map, and the
    // material just goes without it.
    const auto texture = [&]( const aiString& name ) -> Ref<Texture2D>
    {
        auto iter = modelData.mTexturesData.find( modelDirectory / name.C_Str() );
        if ( iter == modelData.mTexturesData.end() )
            return nullptr;
        return uploadTexture( iter->second );
    };

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
                material.mDiffuseMap = texture( str );
                break;
            case aiTextureType_SPECULAR:
                material.mSpecularMap = texture( str );
                break;
            case aiTextureType_NORMALS:
                material.mNormalMap = texture( str );
                break;
            case aiTextureType_HEIGHT:
                if ( not material.mNormalMap )
                    material.mNormalMap = texture( str );
                break;
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
                  const ModelData& modelData,
                  const TextureUploader& uploadTexture )
{
    const aiScene* scene = modelData.mImporter->GetScene();
    VertexBufferData vertices;

    // Every attribute is sized to the vertex count, present in the source or not.
    //
    // A WebGPU pipeline has to provide every input its shader declares, and the
    // lit shaders declare all five. OpenGL was happy to leave an attribute
    // enabled but unfilled - it read whatever was there and the shader ignored
    // it - so a mesh with no tangents still drew. Here it fails pipeline
    // creation outright, which is what "Location[3] is not provided by the
    // previous stage outputs" means.
    vertices.mPositions.resize( mesh->mNumVertices );
    vertices.mNormals.resize( mesh->mNumVertices );
    memmove( vertices.mPositions.data(), mesh->mVertices, sizeof( vec3 ) * vertices.mPositions.size() );
    // aiProcess_GenSmoothNormals should have produced these, but a mesh that
    // arrives without them would otherwise memmove from a null pointer.
    if ( mesh->HasNormals() )
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

    // Tangents and Bitangents.
    //
    // Left zeroed when the source has none. Assimp only computes a tangent
    // basis for a mesh that has texture coordinates, and a mesh without those
    // cannot be normal mapped anyway - the material's normalMapping flag stays
    // off, so the shader never reads them. What matters is that the attribute
    // exists, because the pipeline declares it.
    vertices.mTangents.resize( mesh->mNumVertices );
    vertices.mBitangents.resize( mesh->mNumVertices );
    if ( mesh->HasTangentsAndBitangents() )
    {
        memmove( vertices.mTangents.data(), mesh->mTangents, sizeof( vec3 ) * vertices.mTangents.size() );
        memmove( vertices.mBitangents.data(), mesh->mBitangents, sizeof( vec3 ) * vertices.mBitangents.size() );
    }

    // Material
    aiMaterial* assimp_material = scene->mMaterials[mesh->mMaterialIndex];
    BasicMaterial material = LoadMaterial( assimp_material, modelData, uploadTexture );

    MeshSkin skin;
    if ( modelData.mSkeleton )
        skin = ImportMeshSkin( mesh, *modelData.mSkeleton->mSkeleton, modelData.mPath );

    return Mesh( mesh->mName.C_Str(),
                 std::move( material ),
                 std::move( vertices ),
                 std::move( indices ),
                 std::move( skin ) );
}


Scope<MeshTreeViewNode> ProcessNode( Model& model,
                                     const aiNode* node,
                                     const ModelData& modelData,
                                     const TextureUploader& uploadTexture )
{
    const aiScene* scene = modelData.mImporter->GetScene();
    auto mesh_node = CreateScope<MeshTreeViewNode>( node->mName.C_Str() );

    for ( u32 i = 0; i < node->mNumMeshes; i++ )
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        model.mMeshes.push_back( ProcessMesh( mesh, modelData, uploadTexture ) );
        mesh_node->mMeshes.push_back( &model.mMeshes.back() );
    }
    for ( u32 i = 0; i < node->mNumChildren; i++ )
        mesh_node->mChildren.push_back( ProcessNode( model, node->mChildren[i], modelData, uploadTexture ) );

    return std::move( mesh_node );
}


Ref<Model> LoadModel( const ModelData& modelData, const TextureUploader& uploadTexture )
{
    auto scene = modelData.mImporter->GetScene();

    auto model = CreateRef<Model>();
    model->mName = modelData.mPath.stem().string();
    model->mPath = modelData.mPath;
    model->mMeshes.reserve( scene->mNumMeshes );
    model->mRootMeshTreeView = ProcessNode( *model, scene->mRootNode, modelData, uploadTexture );
    model->mBBox = Model::CreateBoundingBox( *model );
    if ( modelData.mSkeleton )
    {
        model->mSkeleton = modelData.mSkeleton->mSkeleton;
        model->mClips = modelData.mSkeleton->mClips;
    }
    return model;
}


// A model outside any Loader - the engine's own error model. Textures are
// still shared between its meshes, just not with anything else.
Ref<Model> LoadModel( const path& modelPath )
{
    auto modelDataMabe = OpenModel( modelPath );
    if ( not modelDataMabe )
        return nullptr;

    hash_map<path, Ref<Texture2D>> textures;
    return LoadModel( *modelDataMabe, [&]( const TextureData& textureData )
    {
        auto iter = textures.find( textureData.mPath );
        if ( iter == textures.end() )
            iter = textures.emplace( textureData.mPath, LoadTexture2D( textureData ) ).first;
        return iter->second;
    } );
}


Ref<Model> Loader::LoadModel( const path& modelPath )
{
    auto [relPath, absPath] = RelAbsFromProjectPath( modelPath );

    auto iter = mModels.find( relPath );
    if ( iter != mModels.end() )
        return iter->second;

    auto modelData = OpenModel( absPath );
    if ( not modelData )
    {
        LogError( "Failed to load model: {}", absPath.string() );
        return nullptr;
    }

    auto model = bubble::LoadModel( *modelData, [this]( const TextureData& textureData )
    {
        return UploadTexture2D( textureData );
    } );
    mModels.emplace( relPath, model );
    return model;
}


void Loader::LoadModels( const vector<path>& modelsPaths )
{
    ThreadPool threadPool;
    std::vector<FixedSizePackagedTask<pair<path,std::optional<ModelData>>(), 128>> modelDataTasks;

    for ( const path& modelPath : modelsPaths )
    {
        auto [relPath, absPath] = RelAbsFromProjectPath( modelPath );

        if ( mModels.contains( relPath ) )
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
        if ( not modelData )
        {
            LogError( "Failed to load model: {}", relModelPath.string() );
            continue;
        }
        mModels[relModelPath] = bubble::LoadModel( *modelData, [this]( const TextureData& textureData )
        {
            return UploadTexture2D( textureData );
        } );
    }
}

}