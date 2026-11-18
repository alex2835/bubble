#include "bubble/import/model_importer.hpp"
#ifdef BUBBLE_IMPORT_MODELS
#include "bubble/animation/animation_assets.hpp"
#include "bubble/assets/model_asset.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/data/value_json.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/matrix.hpp"
#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <meshoptimizer.h>
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/animation_optimizer.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/raw_track.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/offline/track_builder.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/skeleton_utils.h>
#include <ozz/animation/runtime/track.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

namespace bubble
{
namespace
{
using ozz::animation::offline::RawAnimation;
using ozz::animation::offline::RawSkeleton;

struct ModelSettings
{
    vector<f32> mLods = { 0.5f, 0.25f };
    f32 mScale = 1;
    // The joint whose travel across the ground comes out of the clips.
    string mRootMotion;
};

expected<ModelSettings, string> TryReadSettings( const Json& json )
{
    const ValueType number = ValueType::Of( ValueKind::Number );
    const ValueType type =
        ValueType::RecordOf( { { Name::Declare( "lods" ), ValueType::ListOf( number ) },
                               { Name::Declare( "scale" ), number },
                               { Name::Declare( "root_motion" ), ValueType::Of( ValueKind::Text ) } } );
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
        if ( field.mName == "root_motion"_n )
        {
            settings.mRootMotion = field.mValue.TryAs<Text>()->String();
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

// assimp's rows are glm's columns.
mat4 ToMat4( const aiMatrix4x4& m )
{
    return mat4( m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3, m.c3, m.d3, m.a4, m.b4, m.c4, m.d4 );
}

// A transform of the unscaled model as it is in the scaled one: the
// translation grows, the turn does not.
mat4 Scaled( mat4 matrix, f32 scale )
{
    matrix[3] = vec4( vec3( matrix[3] ) * scale, 1.0f );
    return matrix;
}

mat4 ToMat4( const ozz::math::Transform& transform )
{
    const quat rotation( transform.rotation.w, transform.rotation.x, transform.rotation.y, transform.rotation.z );
    return glm::scale( glm::translate( mat4( 1.0f ), vec3( transform.translation.x, transform.translation.y,
                                                           transform.translation.z ) ) *
                           glm::mat4_cast( rotation ),
                       vec3( transform.scale.x, transform.scale.y, transform.scale.z ) );
}

// What an archive of ozz's made in memory holds.
template <typename T>
string ArchiveOf( const T& object )
{
    ozz::io::MemoryStream stream;
    {
        ozz::io::OArchive archive( &stream );
        archive << object;
    }
    string bytes( stream.Size(), '\0' );
    stream.Seek( 0, ozz::io::Stream::kSet );
    stream.Read( bytes.data(), bytes.size() );
    return bytes;
}

// The positions meshoptimizer reads: the first three floats of each vertex.
const f32* Positions( const vector<ModelVertex>& vertices )
{
    return &vertices.front().mPosition.x;
}

// A mesh as read, before meshoptimizer has ordered it.
struct RawMesh
{
    string mMaterial;
    vector<ModelVertex> mVertices;
    vector<ModelSkinVertex> mSkin;
    vector<u32> mIndices;
};

// One assimp mesh - triangles only -, its positions scaled and then moved
// by `transform`, its normals turned with them.
RawMesh ReadMesh( const aiMesh& source, const aiScene& scene, const mat4& transform, f32 scale )
{
    RawMesh mesh;
    mesh.mMaterial = scene.mMaterials[source.mMaterialIndex]->GetName().C_Str();
    const mat3 turn = glm::transpose( glm::inverse( mat3( transform ) ) );
    mesh.mVertices.resize( source.mNumVertices );
    for ( u32 i = 0; i < source.mNumVertices; ++i )
    {
        const aiVector3D& position = source.mVertices[i];
        ModelVertex& vertex = mesh.mVertices[i];
        vertex.mPosition = vec3( transform * vec4( vec3( position.x, position.y, position.z ) * scale, 1.0f ) );
        if ( source.HasNormals() )
        {
            const vec3 normal = turn * vec3( source.mNormals[i].x, source.mNormals[i].y, source.mNormals[i].z );
            const f32 length = glm::length( normal );
            vertex.mNormal = length > 0 ? normal / length : normal;
        }
        if ( source.HasTextureCoords( 0 ) )
            vertex.mUv = vec2( source.mTextureCoords[0][i].x, source.mTextureCoords[0][i].y );
    }
    mesh.mIndices.reserve( size_t{ source.mNumFaces } * 3 );
    for ( u32 f = 0; f < source.mNumFaces; ++f )
        if ( source.mFaces[f].mNumIndices == 3 )
            for ( u32 corner = 0; corner < 3; ++corner )
                mesh.mIndices.push_back( source.mFaces[f].mIndices[corner] );
    return mesh;
}

// A mesh ordered for the GPU's caches - its skin beside its vertices - and
// its levels made.
ModelMesh Optimize( RawMesh raw, const ModelSettings& settings )
{
    vector<u32>& indices = raw.mIndices;
    // meshoptimizer's C interface reads the floats of the vertices as they lie.
    meshopt_optimizeVertexCache( indices.data(), indices.data(), indices.size(), raw.mVertices.size() );
    meshopt_optimizeOverdraw( indices.data(), indices.data(), indices.size(), Positions( raw.mVertices ),
                              raw.mVertices.size(), sizeof( ModelVertex ), 1.05f );
    vector<u32> remap( raw.mVertices.size() );
    const size_t used =
        meshopt_optimizeVertexFetchRemap( remap.data(), indices.data(), indices.size(), raw.mVertices.size() );
    meshopt_remapIndexBuffer( indices.data(), indices.data(), indices.size(), remap.data() );

    ModelMesh mesh{ .mMaterial = std::move( raw.mMaterial ), .mVertices = {}, .mLods = {} };
    mesh.mVertices.resize( used );
    meshopt_remapVertexBuffer( mesh.mVertices.data(), raw.mVertices.data(), raw.mVertices.size(), sizeof( ModelVertex ),
                               remap.data() );
    if ( not raw.mSkin.empty() )
    {
        mesh.mSkin.resize( used );
        meshopt_remapVertexBuffer( mesh.mSkin.data(), raw.mSkin.data(), raw.mSkin.size(), sizeof( ModelSkinVertex ),
                                   remap.data() );
    }
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

bool Usable( const aiMesh& mesh )
{
    return ( mesh.mPrimitiveTypes & aiPrimitiveType_TRIANGLE ) != 0 and mesh.mNumVertices > 0 and mesh.mNumFaces > 0;
}

// The skeleton a skin is bound to: the nodes that are bones, and every node
// above one - a joint's place in the model is the product of every node
// over it, and that is the space a bone's offset matrix brings a vertex
// back from. Dropping the armature node over the hips would move every
// skinned vertex by its transform. In ozz's order: parents first.
struct Rig
{
    ozz::unique_ptr<ozz::animation::Skeleton> mSkeleton;
    // The first joint of each name.
    hmap<string, u16> mJointOf;
    vector<ModelJoint> mJoints;
};

using Bones = hmap<string, const aiBone*>;

// Every bone of every mesh by name, with the offset matrix of the first mesh
// it moves: assimp's bones are a mesh's, and a mesh split by material is
// several meshes.
Bones CollectBones( const aiScene& scene )
{
    Bones bones;
    for ( u32 m = 0; m < scene.mNumMeshes; ++m )
        for ( u32 b = 0; b < scene.mMeshes[m]->mNumBones; ++b )
            bones.try_emplace( scene.mMeshes[m]->mBones[b]->mName.C_Str(), scene.mMeshes[m]->mBones[b] );
    return bones;
}

bool HasBoneBelow( const aiNode& node, const Bones& bones )
{
    if ( bones.contains( string_view( node.mName.C_Str() ) ) )
        return true;
    for ( u32 i = 0; i < node.mNumChildren; ++i )
        if ( HasBoneBelow( *node.mChildren[i], bones ) )
            return true;
    return false;
}

void FillJoint( RawSkeleton::Joint& joint, const aiNode& node, const Bones& bones, f32 scale )
{
    joint.name = node.mName.C_Str();
    aiVector3D scaling;
    aiVector3D translation;
    aiQuaternion rotation;
    node.mTransformation.Decompose( scaling, rotation, translation );
    joint.transform = ozz::math::Transform{ { translation.x * scale, translation.y * scale, translation.z * scale },
                                            { rotation.x, rotation.y, rotation.z, rotation.w },
                                            { scaling.x, scaling.y, scaling.z } };
    for ( u32 i = 0; i < node.mNumChildren; ++i )
        if ( HasBoneBelow( *node.mChildren[i], bones ) )
            FillJoint( joint.children.emplace_back(), *node.mChildren[i], bones, scale );
}

expected<Rig, string> TryMakeRig( const aiScene& scene, const Bones& bones, f32 scale )
{
    RawSkeleton raw;
    FillJoint( raw.roots.emplace_back(), *scene.mRootNode, bones, scale );
    if ( not raw.Validate() )
        return unexpected( format( "its skeleton has {} joints, past the {} there may be", raw.num_joints(),
                                   static_cast<i32>( ozz::animation::Skeleton::kMaxJoints ) ) );
    Rig rig;
    rig.mSkeleton = ozz::animation::offline::SkeletonBuilder()( raw );
    if ( not rig.mSkeleton )
        return unexpected( "its skeleton would not build"s );

    const ozz::animation::Skeleton& skeleton = *rig.mSkeleton;
    const auto names = skeleton.joint_names();
    const auto parents = skeleton.joint_parents();
    for ( i32 j = 0; j < skeleton.num_joints(); ++j )
    {
        ModelJoint joint{ .mName = names[j],
                          .mRest = ToMat4( ozz::animation::GetJointRestPoseLocalSpace( skeleton, j ) ),
                          .mInverseBind = mat4( 1.0f ) };
        if ( parents[j] != ozz::animation::Skeleton::kNoParent )
            joint.mRest = rig.mJoints[parents[j]].mRest * joint.mRest;
        if ( const auto bone = bones.find( joint.mName ); bone != bones.end() )
            joint.mInverseBind = Scaled( ToMat4( bone->second->mOffsetMatrix ), scale );
        rig.mJointOf.try_emplace( joint.mName, static_cast<u16>( j ) );
        rig.mJoints.push_back( std::move( joint ) );
    }
    return rig;
}

// A mesh's own bones as each vertex's four heaviest joints. assimp's
// LimitBoneWeights has left four at most, their weights adding up to one.
vector<ModelSkinVertex> ReadSkin( const aiMesh& mesh, const Rig& rig )
{
    vector<ModelSkinVertex> skin( mesh.mNumVertices );
    for ( u32 b = 0; b < mesh.mNumBones; ++b )
    {
        const aiBone& bone = *mesh.mBones[b];
        const u16 joint = rig.mJointOf.find( string_view( bone.mName.C_Str() ) )->second;
        for ( u32 w = 0; w < bone.mNumWeights; ++w )
        {
            const aiVertexWeight& weight = bone.mWeights[w];
            ModelSkinVertex& vertex = skin[weight.mVertexId];
            // Into the lightest slot, if it is lighter.
            i32 slot = 0;
            for ( i32 s = 1; s < 4; ++s )
                if ( vertex.mWeights[s] < vertex.mWeights[slot] )
                    slot = s;
            if ( weight.mWeight <= vertex.mWeights[slot] )
                continue;
            vertex.mWeights[slot] = weight.mWeight;
            vertex.mJoints[static_cast<size_t>( slot )] = joint;
        }
    }
    // Weights that do not add up to one would grow or shrink their vertex.
    for ( ModelSkinVertex& vertex : skin )
    {
        const f32 sum = vertex.mWeights.x + vertex.mWeights.y + vertex.mWeights.z + vertex.mWeights.w;
        if ( sum > 0 )
            vertex.mWeights /= sum;
    }
    return skin;
}

// The nearest joint at or above a node: what a mesh without bones of its own
// moves with.
opt<u16> TryJointOver( const aiNode* node, const Rig& rig )
{
    for ( ; node != nullptr; node = node->mParent )
        if ( const auto found = rig.mJointOf.find( string_view( node->mName.C_Str() ) ); found != rig.mJointOf.end() )
            return found->second;
    return nullopt;
}

// The meshes of a skinned model's node and of every node under it, `parent`
// the model's transform of the node's parent. A mesh with bones stays where
// it was bound - its node's transform is the bind's business -, and one
// without moves rigidly with the joint over it: bound to that joint alone,
// at rest where its node puts it. One with no joint over it is placed by
// its node.
void ReadNode( const aiNode& node,
               const aiScene& scene,
               const mat4& parent,
               const Rig& rig,
               f32 scale,
               hset<u32>& skinnedRead,
               vector<RawMesh>& meshes )
{
    const mat4 global = parent * ToMat4( node.mTransformation );
    for ( u32 i = 0; i < node.mNumMeshes; ++i )
    {
        const u32 index = node.mMeshes[i];
        const aiMesh& source = *scene.mMeshes[index];
        if ( not Usable( source ) )
            continue;
        if ( source.mNumBones > 0 )
        {
            // Bound once, however many nodes show it.
            if ( not skinnedRead.insert( index ).second )
                continue;
            RawMesh mesh = ReadMesh( source, scene, mat4( 1.0f ), scale );
            mesh.mSkin = ReadSkin( source, rig );
            meshes.push_back( std::move( mesh ) );
            continue;
        }
        const opt<u16> jointMaybe = TryJointOver( &node, rig );
        if ( not jointMaybe )
        {
            meshes.push_back( ReadMesh( source, scene, Scaled( global, scale ), scale ) );
            continue;
        }
        const ModelJoint& joint = rig.mJoints[*jointMaybe];
        const mat4 bound = glm::inverse( joint.mInverseBind ) * glm::inverse( joint.mRest ) * Scaled( global, scale );
        RawMesh mesh = ReadMesh( source, scene, bound, scale );
        mesh.mSkin.assign( mesh.mVertices.size(),
                           ModelSkinVertex{ .mJoints = { *jointMaybe, 0, 0, 0 }, .mWeights = vec4( 1, 0, 0, 0 ) } );
        meshes.push_back( std::move( mesh ) );
    }
    for ( u32 i = 0; i < node.mNumChildren; ++i )
        ReadNode( *node.mChildren[i], scene, global, rig, scale, skinnedRead, meshes );
}

// Times come in ticks; ozz wants seconds, increasing, within the clip.
// assimp's importers promise none of that - glTF's hands out its last key a
// rounding error past the end -, so every key is clamped, and one that does
// not move time on is dropped.
template <typename Key, typename AiKey, typename Convert>
void FillKeys( ozz::vector<Key>& keys,
               const AiKey* aiKeys,
               u32 count,
               f64 ticksPerSecond,
               f32 duration,
               Convert convert )
{
    keys.reserve( count );
    f32 previous = -1.0f;
    for ( u32 i = 0; i < count; ++i )
    {
        const f32 time = clamp( static_cast<f32>( aiKeys[i].mTime / ticksPerSecond ), 0.0f, duration );
        if ( time <= previous )
            continue;
        keys.push_back( Key{ time, convert( aiKeys[i].mValue ) } );
        previous = time;
    }
}

bool Near( f32 a, f32 b )
{
    return abs( a - b ) <= 1e-4f * max( 1.0f, abs( b ) );
}

bool Uniform( const ozz::math::Float3& v )
{
    return Near( v.y, v.x ) and Near( v.z, v.x );
}

// A joint that holds one uniform scale of its own for the whole clip, other
// than its rest's - Mixamo's hips at a hundred, the clip made in
// centimetres - grows everything under it. The scale goes, and its
// translations shrink by as much, so the clip plays at the model's size,
// nothing sliding.
void DropHeldScale( RawAnimation& raw, const ozz::animation::Skeleton& skeleton )
{
    for ( size_t j = 0; j < raw.tracks.size(); ++j )
    {
        RawAnimation::JointTrack& track = raw.tracks[j];
        if ( track.scales.empty() )
            continue;
        const ozz::math::Float3 held = track.scales.front().value;
        if ( not Uniform( held ) )
            continue;
        const bool constant = all_of( track.scales.begin(), track.scales.end(), [&]( const auto& key ) {
            return Near( key.value.x, held.x ) and Near( key.value.y, held.y ) and Near( key.value.z, held.z );
        } );
        const ozz::math::Transform rest = ozz::animation::GetJointRestPoseLocalSpace( skeleton, static_cast<i32>( j ) );
        if ( not constant or not Uniform( rest.scale ) or rest.scale.x == 0 or Near( held.x, rest.scale.x ) )
            continue;
        const f32 factor = held.x / rest.scale.x;
        for ( auto& key : track.translations )
            key.value = key.value / factor;
        track.scales.assign( 1, RawAnimation::ScaleKey{ 0.0f, rest.scale } );
    }
}

// How a rig's root motion comes out of its clips: its joint's travel across
// the ground and its turn about the model's up, from where it rests, put in
// tracks of their own in the model's space - though the joint's keys are in
// its parent's, which may stand another way up (a Z-up file turned Y-up
// above it).
struct RootMotion
{
    i32 mJoint = 0;
    quat mParentTurn{ 1.0f, 0.0f, 0.0f, 0.0f };
    vec3 mParentScale{ 1.0f };
};

expected<RootMotion, string> TryRootMotion( const Rig& rig, string_view jointName )
{
    const auto found = rig.mJointOf.find( jointName );
    if ( found == rig.mJointOf.end() )
    {
        string names;
        for ( const ModelJoint& joint : rig.mJoints )
            names += ( names.empty() ? "" : ", " ) + joint.mName;
        return unexpected(
            format( "settings.root_motion: no joint {} in its skeleton (joints: {})", jointName, names ) );
    }
    RootMotion motion;
    motion.mJoint = found->second;
    const i16 parent = rig.mSkeleton->joint_parents()[found->second];
    if ( parent != ozz::animation::Skeleton::kNoParent )
    {
        const mat4& rest = rig.mJoints[static_cast<size_t>( parent )].mRest;
        motion.mParentScale =
            vec3( glm::length( vec3( rest[0] ) ), glm::length( vec3( rest[1] ) ), glm::length( vec3( rest[2] ) ) );
        motion.mParentTurn = glm::normalize(
            glm::quat_cast( mat3( vec3( rest[0] ) / motion.mParentScale.x, vec3( rest[1] ) / motion.mParentScale.y,
                                  vec3( rest[2] ) / motion.mParentScale.z ) ) );
    }
    return motion;
}

// The part of `turn` about the unit vector `axis`: what is left when the
// swing that tips the axis is taken off. Projected, not read as Euler angles,
// which take a turn about a Z-up file's up for a roll and, with any lean,
// leave part of the turn in and tip the rest over.
quat TwistAbout( const quat& turn, vec3 axis )
{
    const vec3 along = glm::dot( vec3( turn.x, turn.y, turn.z ), axis ) * axis;
    const quat twist( turn.w, along.x, along.y, along.z );
    const f32 length = glm::length( twist );
    // A half turn about an axis across the up has no twist to speak of.
    return length < 1e-6f ? quat( 1.0f, 0.0f, 0.0f, 0.0f ) : twist / length;
}

ozz::math::Float3 ToOzz( vec3 v )
{
    return { v.x, v.y, v.z };
}

ozz::math::Quaternion ToOzz( const quat& q )
{
    return { q.x, q.y, q.z, q.w };
}

// Takes the root's travel and turn out of the clip, which then plays on the
// spot, and gives them as archived tracks: the travel, then the turn.
pair<string, string> ExtractRoot( RawAnimation& raw,
                                  const ozz::animation::Skeleton& skeleton,
                                  const RootMotion& motion )
{
    RawAnimation::JointTrack& track = raw.tracks[static_cast<size_t>( motion.mJoint )];
    const ozz::math::Transform rest = ozz::animation::GetJointRestPoseLocalSpace( skeleton, motion.mJoint );
    const vec3 restAt( rest.translation.x, rest.translation.y, rest.translation.z );
    const quat restTurn( rest.rotation.w, rest.rotation.x, rest.rotation.y, rest.rotation.z );
    const quat toModel = motion.mParentTurn;
    const quat fromModel = glm::inverse( toModel );
    const vec3 up = fromModel * vec3( 0.0f, 1.0f, 0.0f );
    using Interpolation = ozz::animation::offline::RawTrackInterpolation;

    ozz::animation::offline::RawFloat3Track travels;
    for ( auto& key : track.translations )
    {
        const vec3 at( key.value.x, key.value.y, key.value.z );
        // Across the ground in the model's space - the parent's turn and
        // scale on it -, then back in the parent's to take it off.
        vec3 travel = toModel * ( ( at - restAt ) * motion.mParentScale );
        travel.y = 0.0f;
        travels.keyframes.push_back( { Interpolation::kLinear, key.time / raw.duration, ToOzz( travel ) } );
        key.value = ToOzz( at - ( fromModel * travel ) / motion.mParentScale );
    }
    ozz::animation::offline::RawQuaternionTrack turns;
    quat before( 1.0f, 0.0f, 0.0f, 0.0f );
    for ( auto& key : track.rotations )
    {
        const quat turned( key.value.w, key.value.x, key.value.y, key.value.z );
        quat twist = TwistAbout( turned * glm::inverse( restTurn ), up );
        // On the side of the key before, so they are eased between the short
        // way round.
        if ( glm::dot( twist, before ) < 0.0f )
            twist = -twist;
        before = twist;
        turns.keyframes.push_back(
            { Interpolation::kLinear, key.time / raw.duration, ToOzz( toModel * twist * fromModel ) } );
        // Taken off on the parent's side: what stays is the lean and the rest.
        key.value = ToOzz( glm::inverse( twist ) * turned );
    }
    const ozz::animation::offline::TrackBuilder build;
    return { ArchiveOf( *build( travels ) ), ArchiveOf( *build( turns ) ) };
}

expected<ImportedClip, string> TryImportClip( const aiAnimation& animation,
                                              const ozz::animation::Skeleton& skeleton,
                                              f32 scale,
                                              OptRef<const RootMotion> rootMaybe )
{
    // 0 is "not said", and assimp's own viewers take 25 then.
    const f64 ticksPerSecond = animation.mTicksPerSecond != 0.0 ? animation.mTicksPerSecond : 25.0;
    RawAnimation raw;
    raw.name = animation.mName.C_Str();
    // A clip of one key is a pose: it lasts a frame.
    raw.duration = max( static_cast<f32>( animation.mDuration / ticksPerSecond ), 1.0f / 30.0f );

    hmap<string, const aiNodeAnim*> channels;
    for ( u32 c = 0; c < animation.mNumChannels; ++c )
        channels.try_emplace( animation.mChannels[c]->mNodeName.C_Str(), animation.mChannels[c] );
    // A track a joint, in the skeleton's order.
    const auto names = skeleton.joint_names();
    raw.tracks.resize( names.size() );
    for ( size_t j = 0; j < names.size(); ++j )
    {
        RawAnimation::JointTrack& track = raw.tracks[j];
        const auto channel = channels.find( string_view( names[j] ) );
        if ( channel == channels.end() )
        {
            // An empty track is the identity, not the rest: a joint the clip
            // leaves alone stays where it stands.
            const ozz::math::Transform rest =
                ozz::animation::GetJointRestPoseLocalSpace( skeleton, static_cast<i32>( j ) );
            track.translations.push_back( { 0.0f, rest.translation } );
            track.rotations.push_back( { 0.0f, rest.rotation } );
            track.scales.push_back( { 0.0f, rest.scale } );
            continue;
        }
        const aiNodeAnim& keys = *channel->second;
        FillKeys(
            track.translations, keys.mPositionKeys, keys.mNumPositionKeys, ticksPerSecond, raw.duration,
            [scale]( const aiVector3D& v ) { return ozz::math::Float3( v.x * scale, v.y * scale, v.z * scale ); } );
        FillKeys( track.rotations, keys.mRotationKeys, keys.mNumRotationKeys, ticksPerSecond, raw.duration,
                  []( const aiQuaternion& q ) { return ozz::math::Quaternion( q.x, q.y, q.z, q.w ); } );
        FillKeys( track.scales, keys.mScalingKeys, keys.mNumScalingKeys, ticksPerSecond, raw.duration,
                  []( const aiVector3D& v ) { return ozz::math::Float3( v.x, v.y, v.z ); } );
    }
    if ( not raw.Validate() )
        return unexpected( format( "its clip {} is not one ozz takes", raw.name.c_str() ) );
    DropHeldScale( raw, skeleton );

    // The root's travel out of the clip, which plays on the spot.
    pair<string, string> rootTracks;
    if ( rootMaybe )
        rootTracks = ExtractRoot( raw, skeleton, *rootMaybe );

    // Keys the interpolation gives back anyway go - a millimetre at the
    // joint, ozz's default -: glTF's exporters key every frame.
    RawAnimation optimized;
    if ( not ozz::animation::offline::AnimationOptimizer()( raw, skeleton, &optimized ) )
        optimized = std::move( raw );
    const auto built = ozz::animation::offline::AnimationBuilder()( optimized );
    if ( not built )
        return unexpected( format( "its clip {} would not build", optimized.name.c_str() ) );
    return ImportedClip{ .mName = optimized.name.c_str(),
                         .mDuration = built->duration(),
                         .mAnimation = ArchiveOf( *built ),
                         .mRootTranslation = std::move( rootTracks.first ),
                         .mRootRotation = std::move( rootTracks.second ) };
}

// Where a vertex is at rest: through its skin, if it has one.
vec3 RestPosition( const ModelMesh& mesh, size_t vertex, const vector<mat4>& restSkin )
{
    const vec3 position = mesh.mVertices[vertex].mPosition;
    if ( mesh.mSkin.empty() )
        return position;
    const ModelSkinVertex& skin = mesh.mSkin[vertex];
    vec3 moved( 0.0f );
    for ( size_t slot = 0; slot < 4; ++slot )
        if ( skin.mWeights[static_cast<i32>( slot )] > 0 )
            moved +=
                skin.mWeights[static_cast<i32>( slot )] * vec3( restSkin[skin.mJoints[slot]] * vec4( position, 1.0f ) );
    return moved;
}

expected<ImportedAsset, string> TryImportModel( string_view source, const Json& settingsJson, const AssetPath& path )
{
    BUBBLE_PROFILE_ZONE();
    const auto settingsMaybe = TryReadSettings( settingsJson );
    if ( not settingsMaybe )
        return unexpected( format( "{}: {}", path.View(), settingsMaybe.error() ) );
    const ModelSettings& settings = *settingsMaybe;

    Assimp::Importer importer;
    // FBX's pivots would be "$AssimpFbx$" nodes between every bone and its
    // parent - joints of the skeleton.
    importer.SetPropertyBool( AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false );
    // UVs from the top, as images are; four joints a vertex at most.
    const u32 steps = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals |
                      aiProcess_SortByPType | aiProcess_FlipUVs | aiProcess_LimitBoneWeights |
                      aiProcess_ValidateDataStructure;
    const string hint( path.Extension().substr( 1 ) );
    const aiScene* scene = importer.ReadFileFromMemory( source.data(), source.size(), steps, hint.c_str() );
    if ( scene == nullptr or ( scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ) != 0 or scene->mRootNode == nullptr )
        return unexpected( format( "{}: {}", path.View(), importer.GetErrorString() ) );

    const Bones bones = CollectBones( *scene );
    opt<Rig> rigMaybe;
    if ( not bones.empty() )
    {
        auto madeMaybe = TryMakeRig( *scene, bones, settings.mScale );
        if ( not madeMaybe )
            return unexpected( format( "{}: {}", path.View(), madeMaybe.error() ) );
        rigMaybe = std::move( *madeMaybe );
    }

    vector<RawMesh> raws;
    if ( rigMaybe )
    {
        hset<u32> skinnedRead;
        ReadNode( *scene->mRootNode, *scene, mat4( 1.0f ), *rigMaybe, settings.mScale, skinnedRead, raws );
    }
    else
    {
        // A model, not a hierarchy: the nodes' transforms into the vertices,
        // the meshes of one material as one.
        scene = importer.ApplyPostProcessing( aiProcess_PreTransformVertices );
        if ( scene == nullptr )
            return unexpected( format( "{}: {}", path.View(), importer.GetErrorString() ) );
        for ( u32 m = 0; m < scene->mNumMeshes; ++m )
            if ( Usable( *scene->mMeshes[m] ) )
                raws.push_back( ReadMesh( *scene->mMeshes[m], *scene, mat4( 1.0f ), settings.mScale ) );
    }
    ModelAsset model;
    for ( RawMesh& raw : raws )
        model.mMeshes.push_back( Optimize( std::move( raw ), settings ) );
    if ( model.mMeshes.empty() and not rigMaybe )
        return unexpected( format( "{}: no triangles in it", path.View() ) );

    vector<mat4> restSkin;
    if ( rigMaybe )
    {
        model.mJoints = rigMaybe->mJoints;
        for ( const ModelJoint& joint : model.mJoints )
            restSkin.push_back( joint.mRest * joint.mInverseBind );
    }
    bool first = true;
    for ( const ModelMesh& mesh : model.mMeshes )
        for ( size_t vertex = 0; vertex < mesh.mVertices.size(); ++vertex )
        {
            const vec3 p = RestPosition( mesh, vertex, restSkin );
            model.mMin = first ? p : glm::min( model.mMin, p );
            model.mMax = first ? p : glm::max( model.mMax, p );
            first = false;
        }

    ImportedAsset imported( WriteModelAsset( model ) );
    if ( not rigMaybe )
    {
        if ( not settings.mRootMotion.empty() )
            return unexpected( format( "{}: settings.root_motion: it has no skin, and so no skeleton", path.View() ) );
        return imported;
    }
    opt<RootMotion> rootMaybe;
    if ( not settings.mRootMotion.empty() )
    {
        auto motionMaybe = TryRootMotion( *rigMaybe, settings.mRootMotion );
        if ( not motionMaybe )
            return unexpected( format( "{}: {}", path.View(), motionMaybe.error() ) );
        rootMaybe = std::move( *motionMaybe );
    }
    vector<ImportedClip> clips;
    for ( u32 a = 0; a < scene->mNumAnimations; ++a )
    {
        auto clipMaybe = TryImportClip( *scene->mAnimations[a], *rigMaybe->mSkeleton, settings.mScale,
                                        rootMaybe ? OptRef<const RootMotion>( *rootMaybe ) : nullopt );
        if ( not clipMaybe )
            return unexpected( format( "{}: {}", path.View(), clipMaybe.error() ) );
        // A clip is found by its name: every one has one, and no two the
        // same. glTF leaves them unnamed as often as not.
        const string name = clipMaybe->mName.empty() ? format( "clip_{}", a ) : clipMaybe->mName;
        clipMaybe->mName = name;
        for ( u32 n = 2; any_of( clips.begin(), clips.end(),
                                 [&]( const ImportedClip& clip ) { return clip.mName == clipMaybe->mName; } );
              ++n )
            clipMaybe->mName = format( "{}_{}", name, n );
        clips.push_back( std::move( *clipMaybe ) );
    }
    imported.mMore.emplace( string( cSkeletonKind ), WriteSkeletonAsset( ArchiveOf( *rigMaybe->mSkeleton ) ) );
    imported.mMore.emplace( string( cClipsKind ), WriteClipsAsset( clips ) );
    return imported;
}
}

vector<Importer> ModelImporters()
{
    vector<Importer> importers;
    for ( const string_view extension : { ".glb", ".gltf", ".fbx", ".obj" } )
        importers.push_back( Importer{ .mExtension = string( extension ),
                                       .mKind = string( cModelKind ),
                                       .mMoreKinds = { string( cSkeletonKind ), string( cClipsKind ) },
                                       .mVersion = 3,
                                       .mImport = &TryImportModel } );
    return importers;
}
}
#endif
