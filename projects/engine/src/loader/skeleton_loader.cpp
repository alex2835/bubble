#include "engine/pch/pch.hpp"
#include "engine/loader/loader.hpp"
#include "assimp/scene.h"
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/animation_optimizer.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/skeleton_utils.h>
#include <ozz/animation/runtime/animation.h>

// Assimp scene -> ozz skeleton and clips.
//
// Everything here is CPU work on the aiScene, so it runs inside OpenModel on
// whatever thread decoded the file. The ozz offline builders do in-process
// what the fbx2ozz/gltf2ozz tools would do offline: a RawSkeleton and
// RawAnimations are filled from assimp's nodes and channels, and the builders
// compress them into the runtime types the jobs sample.
//
// The skeleton is the aiNode hierarchy restricted to nodes that are bones or
// have a bone somewhere below them. Keeping the ancestors matters: a joint's
// model space transform is the product of every node above it up to the scene
// root, and that is also the space aiBone::mOffsetMatrix takes a vertex back
// from. Dropping the Armature node above the hips would move every skinned
// vertex by its transform. The extra joints cost a few matrices and carry no
// weights.

namespace bubble
{
namespace
{
using ozz::animation::offline::RawSkeleton;
using ozz::animation::offline::RawAnimation;

mat4 ToGlm( const aiMatrix4x4& m )
{
    // aiMatrix4x4 is row major, glm is column major: transposing the memory
    // image gives the same matrix.
    return glm::transpose( glm::make_mat4( &m.a1 ) );
}

ozz::math::Transform ToOzz( const aiMatrix4x4& m )
{
    aiVector3D scale, translation;
    aiQuaternion rotation;
    m.Decompose( scale, rotation, translation );
    return ozz::math::Transform{ { translation.x, translation.y, translation.z },
                                 { rotation.x, rotation.y, rotation.z, rotation.w },
                                 { scale.x, scale.y, scale.z } };
}


// The names of every node that is a bone, for any mesh, plus the offset
// matrix of the first mesh that references it. Bones are per mesh in assimp;
// the same joint shows up once per mesh that has weights on it, and a glTF
// mesh split by material is several meshes.
struct BoneSet
{
    str_hash_map<const aiBone*> mBones;
    bool Contains( const aiNode* node ) const
    {
        return mBones.contains( string_view( node->mName.C_Str() ) );
    }
};

BoneSet CollectBones( const aiScene* scene )
{
    BoneSet bones;
    for ( u32 m = 0; m < scene->mNumMeshes; m++ )
    {
        const aiMesh* mesh = scene->mMeshes[m];
        for ( u32 b = 0; b < mesh->mNumBones; b++ )
            bones.mBones.try_emplace( mesh->mBones[b]->mName.C_Str(), mesh->mBones[b] );
    }
    return bones;
}


bool HasBoneBelow( const aiNode* node, const BoneSet& bones )
{
    if ( bones.Contains( node ) )
        return true;
    for ( u32 i = 0; i < node->mNumChildren; i++ )
        if ( HasBoneBelow( node->mChildren[i], bones ) )
            return true;
    return false;
}


void FillJoint( RawSkeleton::Joint& joint, const aiNode* node, const BoneSet& bones )
{
    joint.name = node->mName.C_Str();
    joint.transform = ToOzz( node->mTransformation );
    for ( u32 i = 0; i < node->mNumChildren; i++ )
    {
        const aiNode* child = node->mChildren[i];
        if ( not HasBoneBelow( child, bones ) )
            continue;
        joint.children.emplace_back();
        FillJoint( joint.children.back(), child, bones );
    }
}


// Times come in ticks; ozz wants seconds, strictly increasing, within the
// clip. Assimp's importers do not promise any of that (the glTF one hands out
// the last key a rounding error past the duration), so every key is clamped
// and a key that does not advance time is dropped.
template <typename Key, typename AiKey, typename Convert>
void FillKeys( ozz::vector<Key>& keys, const AiKey* aiKeys, u32 count,
               f64 ticksPerSecond, f32 duration, Convert convert )
{
    keys.reserve( count );
    f32 previousTime = -1.0f;
    for ( u32 i = 0; i < count; i++ )
    {
        const f32 time = std::clamp( static_cast<f32>( aiKeys[i].mTime / ticksPerSecond ), 0.0f, duration );
        if ( time <= previousTime )
            continue;
        keys.push_back( Key{ time, convert( aiKeys[i].mValue ) } );
        previousTime = time;
    }
}


Ref<AnimationClip> ImportClip( const aiAnimation* animation,
                               const ozz::animation::Skeleton& skeleton,
                               const path& modelPath )
{
    // Assimp documents 0 as "unspecified", and its own viewers fall back to 25.
    const f64 ticksPerSecond = animation->mTicksPerSecond != 0.0 ? animation->mTicksPerSecond : 25.0;

    RawAnimation raw;
    raw.name = animation->mName.C_Str();
    raw.duration = static_cast<f32>( animation->mDuration / ticksPerSecond );
    if ( raw.duration <= 0.0f )
    {
        LogWarning( "Model: {}. Animation '{}' has no duration, skipped", modelPath.string(), raw.name.c_str() );
        return nullptr;
    }

    // One track per joint, in skeleton order - the builder requires it.
    const auto jointNames = skeleton.joint_names();
    raw.tracks.resize( jointNames.size() );

    str_hash_map<const aiNodeAnim*> channels;
    for ( u32 c = 0; c < animation->mNumChannels; c++ )
        channels.try_emplace( animation->mChannels[c]->mNodeName.C_Str(), animation->mChannels[c] );

    for ( size_t j = 0; j < jointNames.size(); j++ )
    {
        RawAnimation::JointTrack& track = raw.tracks[j];
        auto iter = channels.find( string_view( jointNames[j] ) );
        if ( iter == channels.end() )
        {
            // An empty track samples as identity, not as the rest pose, so a
            // joint the clip does not touch has to be pinned where it stands.
            const ozz::math::Transform rest =
                ozz::animation::GetJointRestPoseLocalSpace( skeleton, static_cast<int>( j ) );
            track.translations.push_back( { 0.0f, rest.translation } );
            track.rotations.push_back( { 0.0f, rest.rotation } );
            track.scales.push_back( { 0.0f, rest.scale } );
            continue;
        }

        const aiNodeAnim* channel = iter->second;
        FillKeys( track.translations, channel->mPositionKeys, channel->mNumPositionKeys, ticksPerSecond, raw.duration,
                  []( const aiVector3D& v ) { return ozz::math::Float3( v.x, v.y, v.z ); } );
        FillKeys( track.rotations, channel->mRotationKeys, channel->mNumRotationKeys, ticksPerSecond, raw.duration,
                  []( const aiQuaternion& q ) { return ozz::math::Quaternion( q.x, q.y, q.z, q.w ); } );
        FillKeys( track.scales, channel->mScalingKeys, channel->mNumScalingKeys, ticksPerSecond, raw.duration,
                  []( const aiVector3D& v ) { return ozz::math::Float3( v.x, v.y, v.z ); } );
    }

    if ( not raw.Validate() )
    {
        LogError( "Model: {}. Animation '{}' failed ozz validation", modelPath.string(), raw.name.c_str() );
        return nullptr;
    }

    // Drops keys the interpolation would reproduce anyway, within 1mm at the
    // joint and 10cm at the end of the chain it moves. glTF exporters bake a
    // key on every frame; this is most of the clip's memory.
    RawAnimation optimized;
    ozz::animation::offline::AnimationOptimizer optimizer;
    if ( not optimizer( raw, skeleton, &optimized ) )
        optimized = std::move( raw );

    auto clip = CreateRef<AnimationClip>();
    clip->mName = optimized.name.c_str();
    clip->mDuration = optimized.duration;
    clip->mAnimation = ozz::animation::offline::AnimationBuilder()( optimized );
    if ( not clip->mAnimation )
    {
        LogError( "Model: {}. Animation '{}' failed to build", modelPath.string(), clip->mName );
        return nullptr;
    }
    // Kept for the additive version, built if a layer ever asks for one.
    // Optimized, so it is a fraction of what came out of the file.
    clip->mRaw = CreateScope<RawAnimation>( std::move( optimized ) );
    return clip;
}

} // namespace


std::optional<SkeletonData> ImportSkeleton( const aiScene* scene, const path& modelPath )
{
    const BoneSet bones = CollectBones( scene );
    if ( bones.mBones.empty() )
        return std::nullopt;

    RawSkeleton raw;
    if ( HasBoneBelow( scene->mRootNode, bones ) )
    {
        raw.roots.emplace_back();
        FillJoint( raw.roots.back(), scene->mRootNode, bones );
    }
    if ( not raw.Validate() )
    {
        LogError( "Model: {}. Skeleton failed ozz validation ({} joints, max {})",
                  modelPath.string(), raw.num_joints(), (int)ozz::animation::Skeleton::kMaxJoints );
        return std::nullopt;
    }

    SkeletonData data;
    data.mSkeleton = CreateRef<Skeleton>();
    data.mSkeleton->mSkeleton = ozz::animation::offline::SkeletonBuilder()( raw );
    if ( not data.mSkeleton->mSkeleton )
    {
        LogError( "Model: {}. Skeleton failed to build", modelPath.string() );
        return std::nullopt;
    }

    const auto jointNames = data.mSkeleton->mSkeleton->joint_names();
    data.mSkeleton->mJointByName.reserve( jointNames.size() );
    data.mSkeleton->mInverseBind.resize( jointNames.size(), glm::identity<mat4>() );
    for ( size_t j = 0; j < jointNames.size(); j++ )
    {
        data.mSkeleton->mJointByName.emplace( jointNames[j], static_cast<u16>( j ) );
        auto bone = bones.mBones.find( string_view( jointNames[j] ) );
        if ( bone != bones.mBones.end() )
            data.mSkeleton->mInverseBind[j] = ToGlm( bone->second->mOffsetMatrix );
    }

    data.mClips.reserve( scene->mNumAnimations );
    for ( u32 a = 0; a < scene->mNumAnimations; a++ )
    {
        auto clip = ImportClip( scene->mAnimations[a], *data.mSkeleton->mSkeleton, modelPath );
        if ( not clip )
            continue;
        // Clips are addressed by name, so every clip gets one and no two
        // share it. glTF leaves animations unnamed more often than not.
        if ( clip->mName.empty() )
            clip->mName = std::format( "clip_{}", a );
        while ( std::ranges::any_of( data.mClips, [&]( const auto& c ) { return c->mName == clip->mName; } ) )
            clip->mName += "_";
        data.mClips.push_back( std::move( clip ) );
    }
    return data;
}


MeshSkin ImportMeshSkin( const aiMesh* mesh, const Skeleton& skeleton, const path& modelPath )
{
    MeshSkin skin;
    if ( mesh->mNumBones == 0 )
        return skin;

    // Four influences per vertex: aiProcess_LimitBoneWeights, which is in the
    // post processing preset OpenModel applies, has already trimmed and
    // renormalised anything heavier.
    skin.mJointIndices.assign( mesh->mNumVertices, glm::u16vec4( 0 ) );
    skin.mJointWeights.assign( mesh->mNumVertices, vec4( 0.0f ) );

    for ( u32 b = 0; b < mesh->mNumBones; b++ )
    {
        const aiBone* bone = mesh->mBones[b];
        // Assimp's bone index is per mesh; the joint index is the skeleton's.
        const auto joint = skeleton.JointIndex( bone->mName.C_Str() );
        if ( not joint )
        {
            LogWarning( "Model: {}. Mesh '{}' bone '{}' is not in the skeleton",
                        modelPath.string(), mesh->mName.C_Str(), bone->mName.C_Str() );
            continue;
        }
        for ( u32 w = 0; w < bone->mNumWeights; w++ )
        {
            const aiVertexWeight& weight = bone->mWeights[w];
            if ( weight.mWeight <= 0.0f )
                continue;
            vec4& weights = skin.mJointWeights[weight.mVertexId];
            glm::u16vec4& joints = skin.mJointIndices[weight.mVertexId];
            // Into the lightest slot; with LimitBoneWeights applied the fifth
            // influence never comes, so this only ever finds an empty one.
            u32 slot = 0;
            for ( u32 s = 1; s < 4; s++ )
                if ( weights[s] < weights[slot] )
                    slot = s;
            if ( weights[slot] >= weight.mWeight )
                continue;
            weights[slot] = weight.mWeight;
            joints[slot] = *joint;
        }
    }

    // Weights that do not sum to one leave a vertex scaled by their sum.
    for ( vec4& weights : skin.mJointWeights )
    {
        const f32 sum = weights.x + weights.y + weights.z + weights.w;
        if ( sum > 0.0f )
            weights /= sum;
    }
    return skin;
}

}
