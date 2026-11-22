#include "bubble/assets/model_asset.hpp"
#include "bubble/assets/asset_registry.hpp"
#include "bubble/data/binary.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
constexpr string_view cMagic = "BMDL";
constexpr u32 cVersion = 3;
// Position, normal, texture coordinate.
constexpr u64 cVertexBytes = 8 * 4;
// Four joints as two u32s, four weights.
constexpr u64 cSkinBytes = 6 * 4;
constexpr u64 cMatrixBytes = 16 * 4;

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

void WriteMatrix( BinaryWriter& out, const mat4& matrix )
{
    for ( i32 column = 0; column < 4; ++column )
        for ( i32 row = 0; row < 4; ++row )
            out.F32( matrix[column][row] );
}

opt<mat4> TryMatrix( BinaryReader& reader )
{
    mat4 matrix( 1.0f );
    for ( i32 column = 0; column < 4; ++column )
        for ( i32 row = 0; row < 4; ++row )
        {
            const auto valueMaybe = reader.TryF32();
            if ( not valueMaybe )
                return nullopt;
            matrix[column][row] = *valueMaybe;
        }
    return matrix;
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
        out.U32( static_cast<u32>( mesh.mSkin.size() ) );
        for ( const ModelSkinVertex& skin : mesh.mSkin )
        {
            out.U32( u32{ skin.mJoints[0] } | ( u32{ skin.mJoints[1] } << 16 ) );
            out.U32( u32{ skin.mJoints[2] } | ( u32{ skin.mJoints[3] } << 16 ) );
            for ( i32 slot = 0; slot < 4; ++slot )
                out.F32( skin.mWeights[slot] );
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
    out.U32( static_cast<u32>( model.mJoints.size() ) );
    for ( const ModelJoint& joint : model.mJoints )
    {
        out.Text( joint.mName );
        WriteMatrix( out, joint.mRest );
        WriteMatrix( out, joint.mInverseBind );
        // One past it: 0 is none.
        out.U32( static_cast<u32>( joint.mParent + 1 ) );
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
        const auto skinMaybe = reader.TryU32();
        if ( not skinMaybe or *skinMaybe * cSkinBytes > reader.Left() )
            return cutShort();
        if ( *skinMaybe != 0 and *skinMaybe != mesh.mVertices.size() )
            return unexpected( format( "mesh {} of a model has a skin of {} vertices, not its {}", m, *skinMaybe,
                                       mesh.mVertices.size() ) );
        mesh.mSkin.resize( *skinMaybe );
        for ( ModelSkinVertex& skin : mesh.mSkin )
        {
            const u32 low = *reader.TryU32();
            const u32 high = *reader.TryU32();
            skin.mJoints = { static_cast<u16>( low & 0xFFFF ), static_cast<u16>( low >> 16 ),
                             static_cast<u16>( high & 0xFFFF ), static_cast<u16>( high >> 16 ) };
            for ( i32 slot = 0; slot < 4; ++slot )
                skin.mWeights[slot] = *reader.TryF32();
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
    const auto jointsMaybe = reader.TryU32();
    if ( not jointsMaybe or *jointsMaybe * cMatrixBytes * 2 > reader.Left() )
        return cutShort();
    for ( u32 j = 0; j < *jointsMaybe; ++j )
    {
        const auto nameMaybe = reader.TryText();
        const auto restMaybe = nameMaybe ? TryMatrix( reader ) : nullopt;
        const auto inverseBindMaybe = restMaybe ? TryMatrix( reader ) : nullopt;
        const auto parentMaybe = inverseBindMaybe ? reader.TryU32() : nullopt;
        if ( not parentMaybe )
            return cutShort();
        const i32 parent = static_cast<i32>( *parentMaybe ) - 1;
        if ( parent >= static_cast<i32>( j ) )
            return unexpected( format( "joint {} hangs from joint {}, not one before it", j, parent ) );
        model.mJoints.push_back( ModelJoint{ .mName = string( *nameMaybe ),
                                             .mRest = *restMaybe,
                                             .mInverseBind = *inverseBindMaybe,
                                             .mParent = parent } );
    }
    for ( u32 m = 0; m < model.mMeshes.size(); ++m )
        for ( const ModelSkinVertex& skin : model.mMeshes[m].mSkin )
            for ( const u16 joint : skin.mJoints )
                if ( joint >= model.mJoints.size() )
                    return unexpected( format( "mesh {} of a model has a vertex on joint {}, past its {} joints", m,
                                               joint, model.mJoints.size() ) );
    if ( not reader.AtEnd() )
        return unexpected( "a model with more after its joints"s );
    return model;
}

void RegisterModelLoader( AssetRegistry& registry )
{
    registry.RegisterLoader<ModelAsset>(
        cModelKind, []( string_view bytes, const AssetPath& ) { return TryReadModelAsset( bytes ); } );
}
}
