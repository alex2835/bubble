#include "bubble/import/model_importer.hpp"
#ifdef BUBBLE_IMPORT_MODELS
#include "bubble/assets/model_asset.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/data/value_json.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <meshoptimizer.h>

namespace bubble
{
namespace
{
struct ModelSettings
{
    vector<f32> mLods = { 0.5f, 0.25f };
    f32 mScale = 1;
};

expected<ModelSettings, string> TryReadSettings( const Json& json )
{
    const ValueType number = ValueType::Of( ValueKind::Number );
    const ValueType type = ValueType::RecordOf(
        { { Name::Declare( "lods" ), ValueType::ListOf( number ) }, { Name::Declare( "scale" ), number } } );
    const AssetCatalog none;
    auto valueMaybe = TryValueFromJson( json, type, "settings", none );
    if ( not valueMaybe )
        return unexpected( std::move( valueMaybe.error() ) );
    ModelSettings settings;
    for ( const ValueField& field : *valueMaybe->TryAs<Value::Record>() )
    {
        if ( field.mName == "scale"_n )
        {
            settings.mScale = static_cast<f32>( *field.mValue.TryAs<f64>() );
            continue;
        }
        settings.mLods.clear();
        for ( const Value& share : *field.mValue.TryAs<Value::List>() )
        {
            const f64 ratio = *share.TryAs<f64>();
            if ( not( ratio > 0 and ratio < 1 ) )
                return unexpected( format( "settings.lods: {} is not a share between 0 and 1", ratio ) );
            settings.mLods.push_back( static_cast<f32>( ratio ) );
        }
    }
    return settings;
}

// The positions meshoptimizer reads: the first three floats of each vertex.
const f32* Positions( const vector<ModelVertex>& vertices )
{
    return &vertices.front().mPosition.x;
}

// One assimp mesh - triangles only - ordered, and its levels made.
ModelMesh MakeMesh( const aiMesh& source, const aiScene& scene, const ModelSettings& settings )
{
    ModelMesh mesh;
    mesh.mMaterial = scene.mMaterials[source.mMaterialIndex]->GetName().C_Str();
    vector<ModelVertex> vertices( source.mNumVertices );
    for ( u32 i = 0; i < source.mNumVertices; ++i )
    {
        const aiVector3D& position = source.mVertices[i];
        vertices[i].mPosition = vec3( position.x, position.y, position.z ) * settings.mScale;
        if ( source.HasNormals() )
            vertices[i].mNormal = vec3( source.mNormals[i].x, source.mNormals[i].y, source.mNormals[i].z );
        if ( source.HasTextureCoords( 0 ) )
            vertices[i].mUv = vec2( source.mTextureCoords[0][i].x, source.mTextureCoords[0][i].y );
    }
    vector<u32> indices;
    indices.reserve( size_t{ source.mNumFaces } * 3 );
    for ( u32 f = 0; f < source.mNumFaces; ++f )
        if ( source.mFaces[f].mNumIndices == 3 )
            for ( u32 corner = 0; corner < 3; ++corner )
                indices.push_back( source.mFaces[f].mIndices[corner] );

    // meshoptimizer's C interface reads the floats of the vertices as they lie.
    meshopt_optimizeVertexCache( indices.data(), indices.data(), indices.size(), vertices.size() );
    meshopt_optimizeOverdraw( indices.data(), indices.data(), indices.size(), Positions( vertices ), vertices.size(),
                              sizeof( ModelVertex ), 1.05f );
    mesh.mVertices.resize( vertices.size() );
    const size_t used = meshopt_optimizeVertexFetch( mesh.mVertices.data(), indices.data(), indices.size(),
                                                     vertices.data(), vertices.size(), sizeof( ModelVertex ) );
    mesh.mVertices.resize( used );
    mesh.mLods.push_back( ModelLod{ .mError = 0, .mIndices = indices } );

    const f32 scale =
        meshopt_simplifyScale( Positions( mesh.mVertices ), mesh.mVertices.size(), sizeof( ModelVertex ) );
    for ( const f32 share : settings.mLods )
    {
        const size_t target = static_cast<size_t>( static_cast<f32>( indices.size() ) * share ) / 3 * 3;
        vector<u32> lod( indices.size() );
        f32 error = 0;
        // Up to a tenth of the model's size astray: past that a level is no
        // longer the same thing.
        lod.resize( meshopt_simplify( lod.data(), indices.data(), indices.size(), Positions( mesh.mVertices ),
                                      mesh.mVertices.size(), sizeof( ModelVertex ), target, 0.1f, 0, &error ) );
        // No coarser than the last: the simplifier got no further.
        if ( lod.empty() or lod.size() >= mesh.mLods.back().mIndices.size() )
            break;
        meshopt_optimizeVertexCache( lod.data(), lod.data(), lod.size(), mesh.mVertices.size() );
        mesh.mLods.push_back( ModelLod{ .mError = error * scale, .mIndices = std::move( lod ) } );
    }
    return mesh;
}

expected<string, string> TryImportModel( string_view source, const Json& settingsJson, const AssetPath& path )
{
    BUBBLE_PROFILE_ZONE();
    const auto settingsMaybe = TryReadSettings( settingsJson );
    if ( not settingsMaybe )
        return unexpected( format( "{}: {}", path.View(), settingsMaybe.error() ) );

    Assimp::Importer importer;
    // The nodes' transforms into the vertices: a model, not a hierarchy - a
    // skeleton's come with animation. UVs from the top, as images are.
    const u32 steps = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals |
                      aiProcess_PreTransformVertices | aiProcess_SortByPType | aiProcess_FlipUVs |
                      aiProcess_ValidateDataStructure;
    const string hint( path.Extension().substr( 1 ) );
    const aiScene* scene = importer.ReadFileFromMemory( source.data(), source.size(), steps, hint.c_str() );
    if ( scene == nullptr or ( scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ) != 0 )
        return unexpected( format( "{}: {}", path.View(), importer.GetErrorString() ) );

    ModelAsset model;
    for ( u32 m = 0; m < scene->mNumMeshes; ++m )
    {
        const aiMesh& mesh = *scene->mMeshes[m];
        if ( ( mesh.mPrimitiveTypes & aiPrimitiveType_TRIANGLE ) == 0 or mesh.mNumVertices == 0 or mesh.mNumFaces == 0 )
            continue;
        model.mMeshes.push_back( MakeMesh( mesh, *scene, *settingsMaybe ) );
    }
    if ( model.mMeshes.empty() )
        return unexpected( format( "{}: no triangles in it", path.View() ) );
    model.mMin = model.mMax = model.mMeshes.front().mVertices.front().mPosition;
    for ( const ModelMesh& mesh : model.mMeshes )
        for ( const ModelVertex& vertex : mesh.mVertices )
        {
            const vec3& p = vertex.mPosition;
            model.mMin = vec3( min( model.mMin.x, p.x ), min( model.mMin.y, p.y ), min( model.mMin.z, p.z ) );
            model.mMax = vec3( max( model.mMax.x, p.x ), max( model.mMax.y, p.y ), max( model.mMax.z, p.z ) );
        }
    return WriteModelAsset( model );
}
}

vector<Importer> ModelImporters()
{
    vector<Importer> importers;
    for ( const string_view extension : { ".glb", ".gltf", ".fbx", ".obj" } )
        importers.push_back( Importer{ .mExtension = string( extension ),
                                       .mKind = string( cModelKind ),
                                       .mVersion = 1,
                                       .mImport = &TryImportModel } );
    return importers;
}
}
#endif
