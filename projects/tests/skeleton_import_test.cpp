#include "test.hpp"
#include "engine/loader/loader.hpp"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/skeleton_utils.h>
#include <ozz/animation/runtime/animation.h>

// The assimp -> ozz import, checked on a Khronos sample that has all three of
// skin, skeleton and clip. OpenModel does no GPU work, so this runs headless.
//
// The one check that matters is the rest pose: the model space transform of
// every joint times its inverse bind matrix must bring a vertex from the
// mesh's space to scene space, i.e. equal the mesh node's own global
// transform (the identity, when the mesh sits at the root). Otherwise the
// hierarchy and the offset matrices disagree - a dropped ancestor, a joint
// index mixed up with assimp's bone index, a transposed matrix. Any of those
// shows here before a single vertex is skinned.

namespace
{
const path cCesiumMan = path( BUBBLE_TEST_RESOURCES ) / "models/CesiumMan.glb";

bool Near( const mat4& a, const mat4& b, f32 epsilon )
{
    for ( int c = 0; c < 4; c++ )
        for ( int r = 0; r < 4; r++ )
            if ( std::abs( a[c][r] - b[c][r] ) > epsilon )
                return false;
    return true;
}

bool NearIdentity( const mat4& m, f32 epsilon )
{
    return Near( m, glm::identity<mat4>(), epsilon );
}

mat4 ToGlm( const aiMatrix4x4& m )
{
    return glm::transpose( glm::make_mat4( &m.a1 ) );
}

// The global transform of the node that draws mesh `meshIndex`.
std::optional<mat4> MeshNodeGlobal( const aiNode* node, u32 meshIndex, const mat4& parent )
{
    const mat4 global = parent * ToGlm( node->mTransformation );
    for ( u32 i = 0; i < node->mNumMeshes; i++ )
        if ( node->mMeshes[i] == meshIndex )
            return global;
    for ( u32 i = 0; i < node->mNumChildren; i++ )
        if ( auto found = MeshNodeGlobal( node->mChildren[i], meshIndex, global ) )
            return found;
    return std::nullopt;
}
}

TEST( SkeletonImport_CesiumMan )
{
    auto modelData = OpenModel( cCesiumMan );
    CHECK( modelData.has_value() );
    if ( not modelData )
        return;

    CHECK( modelData->mSkeleton.has_value() );
    if ( not modelData->mSkeleton )
        return;

    const Skeleton& skeleton = *modelData->mSkeleton->mSkeleton;
    // 19 bones plus the nodes above them.
    CHECK( skeleton.JointCount() >= 19 );
    CHECK( skeleton.mInverseBind.size() == skeleton.JointCount() );
    CHECK( skeleton.mJointByName.size() == skeleton.JointCount() );

    // Rest pose x inverse bind == the mesh node's global transform, for every
    // bone of every mesh.
    const aiScene* scene = modelData->mImporter->GetScene();
    const auto restPose = ozz::animation::GetRestPoseModelSpace( *skeleton.mSkeleton );
    u32 bones = 0;
    for ( u32 m = 0; m < scene->mNumMeshes; m++ )
    {
        const aiMesh* mesh = scene->mMeshes[m];
        const auto meshGlobal = MeshNodeGlobal( scene->mRootNode, m, glm::identity<mat4>() );
        CHECK( meshGlobal.has_value() );
        for ( u32 b = 0; b < mesh->mNumBones; b++ )
        {
            const auto j = skeleton.JointIndex( mesh->mBones[b]->mName.C_Str() );
            CHECK( j.has_value() );
            if ( not j or not meshGlobal )
                continue;
            bones++;
            mat4 model;
            static_assert( sizeof( model ) == sizeof( restPose[*j] ) );
            memcpy( &model, &restPose[*j], sizeof( model ) );
            const mat4 skin = model * skeleton.mInverseBind[*j];
            if ( not Near( skin, *meshGlobal, 1e-3f ) )
            {
                std::println( "  joint {} '{}': rest * inverseBind != mesh node transform",
                              *j, skeleton.mSkeleton->joint_names()[*j] );
                CHECK( false );
            }
        }
    }
    CHECK( bones == 19 );
    string names;
    for ( const char* name : skeleton.mSkeleton->joint_names() )
        names += string( names.empty() ? "" : " " ) + name;
    std::println( "  joints: {}", names );

    // The clip covers every joint and lasts as long as the file says (2s).
    const auto& clips = modelData->mSkeleton->mClips;
    CHECK( clips.size() == 1 );
    if ( clips.empty() )
        return;
    CHECK( clips[0]->mAnimation != nullptr );
    CHECK( not clips[0]->mName.empty() );
    std::println( "  clip '{}' {:.2f}s, {} joints", clips[0]->mName, clips[0]->mDuration, skeleton.JointCount() );
    CHECK( clips[0]->mAnimation->num_tracks() == (int)skeleton.JointCount() );
    CHECK( std::abs( clips[0]->mDuration - 2.0f ) < 0.05f );
    // The additive version builds from the kept keys, once.
    const ozz::animation::Animation* additive = clips[0]->Additive();
    CHECK( additive != nullptr );
    CHECK( additive == clips[0]->Additive() );
    if ( additive )
        CHECK( additive->num_tracks() == clips[0]->mAnimation->num_tracks() );

    // Root motion off the hips: the walk covers ground, in a straight line.
    const auto hips = skeleton.JointIndex( "Skeleton_torso_joint_1" );
    CHECK( hips.has_value() );
    const AnimationClip::RootMotion* motion = clips[0]->WithRootMotion( skeleton, hips ? *hips : -1 );
    CHECK( motion != nullptr );
    if ( motion )
    {
        vec3 travel;
        glm::quat turn;
        motion->Delta( 0.0f, 1.0f, false, travel, turn );
        std::println( "  root travel over the clip: {:.3f} {:.3f} {:.3f}, yaw {:.3f}",
                      travel.x, travel.y, travel.z, glm::angle( turn ) );
        // CesiumMan walks on the spot, so there is no travel to check - but
        // whatever the extractor took has to be horizontal in model space,
        // and this skeleton's root sits under a Z-up conversion, so that is
        // not the same as horizontal in the root's own space.
        CHECK( std::abs( travel.y ) < 1e-4f );
        vec3 p0, p1;
        motion->Sample( 0.0f, p0, turn );
        motion->Sample( 0.5f, p1, turn );
        CHECK( std::abs( p0.y - p1.y ) < 1e-4f );
        // Two halves make the whole, and the same around the seam.
        vec3 a, b, around;
        motion->Delta( 0.0f, 0.5f, false, a, turn );
        motion->Delta( 0.5f, 1.0f, false, b, turn );
        CHECK( glm::length( a + b - travel ) < 1e-3f );
        motion->Delta( 0.75f, 0.25f, true, around, turn );
        vec3 tail, head;
        motion->Delta( 0.75f, 1.0f, false, tail, turn );
        motion->Delta( 0.0f, 0.25f, false, head, turn );
        CHECK( glm::length( around - ( tail + head ) ) < 1e-3f );
        // Cached per joint.
        CHECK( clips[0]->WithRootMotion( skeleton, *hips ) == motion );
    }

    // Every skinned vertex has weights that sum to one and joints in range.
    u32 skinnedVertices = 0;
    for ( u32 m = 0; m < scene->mNumMeshes; m++ )
    {
        const MeshSkin skin = ImportMeshSkin( scene->mMeshes[m], skeleton, cCesiumMan );
        CHECK( skin.mJointIndices.size() == scene->mMeshes[m]->mNumVertices );
        for ( size_t v = 0; v < skin.mJointWeights.size(); v++ )
        {
            const vec4& w = skin.mJointWeights[v];
            CHECK( std::abs( w.x + w.y + w.z + w.w - 1.0f ) < 1e-4f );
            for ( int i = 0; i < 4; i++ )
                CHECK( skin.mJointIndices[v][i] < skeleton.JointCount() );
            skinnedVertices++;
        }
    }
    CHECK( skinnedVertices > 0 );
}
