#include "bubble/assets/model_asset.hpp"
#include "bubble/assets/asset_registry.hpp"
#include "bubble/data/binary.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
constexpr string_view cMagic = "BMDL";
constexpr u32 cVersion = 1;
// Position, normal, texture coordinate.
constexpr u64 cVertexBytes = 8 * 4;

void WriteVec3( BinaryWriter& out, vec3 v )
{
    out.F32( v.x );
    out.F32( v.y );
    out.F32( v.z );
}

opt<vec3> TryVec3( BinaryReader& reader )
{
    const auto xMaybe = reader.TryF32();
    const auto yMaybe = reader.TryF32();
    const auto zMaybe = reader.TryF32();
    if ( not xMaybe or not yMaybe or not zMaybe )
        return nullopt;
    return vec3( *xMaybe, *yMaybe, *zMaybe );
}
}

string WriteModelAsset( const ModelAsset& model )
{
    BinaryWriter out;
    out.Bytes( cMagic );
    out.U32( cVersion );
    WriteVec3( out, model.mMin );
    WriteVec3( out, model.mMax );
    out.U32( static_cast<u32>( model.mMeshes.size() ) );
    for ( const ModelMesh& mesh : model.mMeshes )
    {
        out.Text( mesh.mMaterial );
        out.U32( static_cast<u32>( mesh.mVertices.size() ) );
        for ( const ModelVertex& vertex : mesh.mVertices )
        {
            WriteVec3( out, vertex.mPosition );
            WriteVec3( out, vertex.mNormal );
            out.F32( vertex.mUv.x );
            out.F32( vertex.mUv.y );
        }
        out.U32( static_cast<u32>( mesh.mLods.size() ) );
        for ( const ModelLod& lod : mesh.mLods )
        {
            out.F32( lod.mError );
            out.U32( static_cast<u32>( lod.mIndices.size() ) );
            for ( const u32 index : lod.mIndices )
                out.U32( index );
        }
    }
    return out.Take();
}

expected<ModelAsset, string> TryReadModelAsset( string_view bytes )
{
    BinaryReader reader( bytes );
    const auto cutShort = [] { return unexpected( "a model cut short"s ); };
    const auto magicMaybe = reader.TryBytes( cMagic.size() );
    if ( not magicMaybe or *magicMaybe != cMagic )
        return unexpected( "not a model the importer made"s );
    const auto versionMaybe = reader.TryU32();
    if ( not versionMaybe )
        return cutShort();
    if ( *versionMaybe != cVersion )
        return unexpected( format( "a model of version {}, not {}: import it again", *versionMaybe, cVersion ) );

    ModelAsset model;
    const auto minMaybe = TryVec3( reader );
    const auto maxMaybe = TryVec3( reader );
    const auto meshesMaybe = reader.TryU32();
    if ( not minMaybe or not maxMaybe or not meshesMaybe )
        return cutShort();
    model.mMin = *minMaybe;
    model.mMax = *maxMaybe;
    for ( u32 m = 0; m < *meshesMaybe; ++m )
    {
        ModelMesh mesh;
        const auto materialMaybe = reader.TryText();
        const auto verticesMaybe = reader.TryU32();
        // A count past what is left is no model, and nothing to reserve.
        if ( not materialMaybe or not verticesMaybe or *verticesMaybe * cVertexBytes > reader.Left() )
            return cutShort();
        mesh.mMaterial = string( *materialMaybe );
        mesh.mVertices.resize( *verticesMaybe );
        for ( ModelVertex& vertex : mesh.mVertices )
        {
            const auto positionMaybe = TryVec3( reader );
            const auto normalMaybe = TryVec3( reader );
            const auto uMaybe = reader.TryF32();
            const auto vMaybe = reader.TryF32();
            if ( not positionMaybe or not normalMaybe or not uMaybe or not vMaybe )
                return cutShort();
            vertex =
                ModelVertex{ .mPosition = *positionMaybe, .mNormal = *normalMaybe, .mUv = vec2( *uMaybe, *vMaybe ) };
        }
        const auto lodsMaybe = reader.TryU32();
        if ( not lodsMaybe )
            return cutShort();
        for ( u32 l = 0; l < *lodsMaybe; ++l )
        {
            const auto errorMaybe = reader.TryF32();
            const auto countMaybe = reader.TryU32();
            if ( not errorMaybe or not countMaybe or u64{ *countMaybe } * 4 > reader.Left() )
                return cutShort();
            ModelLod lod{ .mError = *errorMaybe, .mIndices = {} };
            lod.mIndices.reserve( *countMaybe );
            for ( u32 i = 0; i < *countMaybe; ++i )
            {
                const u32 index = *reader.TryU32();
                if ( index >= mesh.mVertices.size() )
                    return unexpected( format( "mesh {} of a model has an index {} past its {} vertices", m, index,
                                               mesh.mVertices.size() ) );
                lod.mIndices.push_back( index );
            }
            mesh.mLods.push_back( std::move( lod ) );
        }
        model.mMeshes.push_back( std::move( mesh ) );
    }
    if ( not reader.AtEnd() )
        return unexpected( "a model with more after its last mesh"s );
    return model;
}

void RegisterModelLoader( AssetRegistry& registry )
{
    registry.RegisterLoader<ModelAsset>(
        cModelKind, []( string_view bytes, const AssetPath& ) { return TryReadModelAsset( bytes ); } );
}
}
