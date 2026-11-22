#include "physics_ragdoll.hpp"
#include "bubble/types/algorithm.hpp"

namespace bubble::physics
{
namespace
{
vec3 Origin( const mat4& matrix )
{
    return vec3( matrix[3] );
}

constexpr f32 cPi = 3.14159265f;

constexpr f32 Degrees( f32 degrees )
{
    return degrees * cPi / 180.0f;
}

string Lower( string_view text )
{
    string lower( text );
    for ( char& letter : lower )
        if ( letter >= 'A' and letter <= 'Z' )
            letter = static_cast<char>( letter - 'A' + 'a' );
    return lower;
}

// A human's limits at the joint a bone of this name hangs from - the names of
// Mixamo's skeleton and those like it -; false for a name it does not know.
bool Human( string_view joint, RagdollBone& bone )
{
    const string name = Lower( joint );
    const auto has = [&name]( string_view part ) { return name.find( part ) != string::npos; };
    const auto set = [&bone]( f32 swing, f32 twist, BoneBend bend = BoneBend::Any ) {
        bone.mSwing = Degrees( swing );
        bone.mTwist = Degrees( twist );
        bone.mBend = bend;
    };
    // The longer names first: a forearm is an arm too, an upper leg a leg.
    if ( has( "forearm" ) or has( "lowerarm" ) )
        set( 10, 10, BoneBend::Elbow );
    else if ( has( "upleg" ) or has( "thigh" ) )
        set( 45, 20 );
    else if ( has( "leg" ) or has( "calf" ) or has( "shin" ) )
        set( 10, 5, BoneBend::Knee );
    else if ( has( "shoulder" ) or has( "clavicle" ) )
        set( 15, 10 );
    else if ( has( "arm" ) )
        set( 60, 35 );
    else if ( has( "spine" ) or has( "chest" ) or has( "hips" ) or has( "pelvis" ) )
        set( 15, 10 );
    else if ( has( "neck" ) )
        set( 25, 25 );
    else if ( has( "head" ) )
        set( 30, 35 );
    else if ( has( "hand" ) or has( "wrist" ) )
        set( 35, 20 );
    else if ( has( "toe" ) )
        set( 15, 5 );
    else if ( has( "foot" ) or has( "ankle" ) )
        set( 25, 10 );
    else
        return false;
    return true;
}

// The pose's rotation alone.
quat Turn( const mat4& pose )
{
    mat3 axes( pose );
    for ( int axis = 0; axis < 3; ++axis )
        axes[axis] = glm::normalize( axes[axis] );
    return glm::normalize( glm::quat_cast( axes ) );
}

// A frame at a body's origin along `axis`, its normal any square to it.
JointFrame AlongAxis( vec3 axis )
{
    const vec3 across = glm::abs( axis.y ) < 0.9f ? glm::cross( axis, vec3( 0.0f, 1.0f, 0.0f ) )
                                                  : glm::cross( axis, vec3( 1.0f, 0.0f, 0.0f ) );
    return JointFrame{ vec3( 0.0f ), axis, glm::normalize( across ) };
}
}

RagdollSkeleton MakeRagdollSkeleton( const ModelAsset& model, const Ragdoll& ragdoll, f32 scale )
{
    RagdollSkeleton skeleton;
    const size_t count = model.mJoints.size();
    skeleton.mBoneOf.assign( count, -1 );
    if ( count == 0 or scale <= 0.0f )
        return skeleton;
    vector<vector<u32>> children( count );
    for ( u32 j = 0; j < count; ++j )
        if ( model.mJoints[j].mParent >= 0 )
            children[static_cast<size_t>( model.mJoints[j].mParent )].push_back( j );

    f32 volume = 0.0f;
    for ( u32 j = 0; j < count; ++j )
    {
        const ModelJoint& joint = model.mJoints[j];
        const mat4 toJoint = glm::inverse( joint.mRest );
        const i32 riding = joint.mParent >= 0 ? skeleton.mBoneOf[static_cast<size_t>( joint.mParent )] : -1;
        // The way it goes on: away from its parent, or up for a root.
        const vec3 onward =
            joint.mParent >= 0
                ? -vec3( toJoint * vec4( Origin( model.mJoints[static_cast<size_t>( joint.mParent )].mRest ), 1.0f ) )
                : mat3( toJoint ) * vec3( 0.0f, 1.0f, 0.0f );
        // The child that goes on most nearly that way.
        opt<vec3> tipMaybe;
        f32 best = -2.0f;
        for ( const u32 child : children[j] )
        {
            const vec3 tip( toJoint * vec4( Origin( model.mJoints[child].mRest ), 1.0f ) );
            if ( glm::length( tip ) <= 0.0f )
                continue;
            const f32 along =
                glm::length( onward ) > 0.0f ? glm::dot( glm::normalize( tip ), glm::normalize( onward ) ) : 1.0f;
            if ( along > best )
            {
                best = along;
                tipMaybe = tip;
            }
        }
        // The first bone always is one, however short: the rest hang on it.
        const bool first = skeleton.mBones.empty();
        if ( not first and ( not tipMaybe or glm::length( *tipMaybe ) * scale < ragdoll.mMinBone ) )
        {
            skeleton.mBoneOf[j] = riding;
            continue;
        }
        vec3 tip = tipMaybe.value_or( vec3( 0.0f, ragdoll.mMinBone / scale, 0.0f ) );
        if ( glm::length( tip ) * scale < ragdoll.mMinBone )
            tip = glm::normalize( tip ) * ragdoll.mMinBone / scale;
        const f32 length = glm::length( tip );
        const f32 radius = max( length * ragdoll.mThickness, 0.01f / scale );
        // By its length alone: by its volume a neck weighs a tenth of a hip,
        // and a solver holds a chain that uneven loosely.
        const f32 size = length;
        volume += size;
        skeleton.mBoneOf[j] = static_cast<i32>( skeleton.mBones.size() );
        RagdollBone made{ .mJoint = j, .mParent = riding, .mTip = tip, .mRadius = radius, .mShare = size };
        if ( not Human( joint.mName, made ) )
        {
            made.mSwing = ragdoll.mSwing;
            made.mTwist = ragdoll.mTwist;
        }
        skeleton.mBones.push_back( made );
    }
    for ( u32 j = 0; j < count; ++j )
        if ( Lower( model.mJoints[j].mName ).find( "toe" ) != string::npos and model.mJoints[j].mParent >= 0 )
        {
            skeleton.mToe = static_cast<i32>( j );
            skeleton.mFoot = model.mJoints[j].mParent;
            break;
        }
    for ( RagdollBone& bone : skeleton.mBones )
        bone.mShare = volume > 0.0f ? bone.mShare / volume : 1.0f / static_cast<f32>( skeleton.mBones.size() );
    return skeleton;
}

BoneJoint MakeBoneJoint( const RagdollSkeleton& skeleton, u32 bone, const vector<mat4>& poses )
{
    const RagdollBone& own = skeleton.mBones[bone];
    const RagdollBone& above = skeleton.mBones[static_cast<size_t>( own.mParent )];
    const quat ownTurn = Turn( poses[own.mJoint] );
    const quat aboveTurn = Turn( poses[above.mJoint] );
    // From the bone's body to the one above's, in the model's space: what
    // takes a frame on the one to the same place on the other.
    const mat4 toAbove =
        glm::inverse( glm::translate( mat4( 1.0f ), Origin( poses[above.mJoint] ) ) * glm::mat4_cast( aboveTurn ) ) *
        glm::translate( mat4( 1.0f ), Origin( poses[own.mJoint] ) ) * glm::mat4_cast( ownTurn );
    BoneJoint made;
    made.mOwn = AlongAxis( glm::normalize( own.mTip ) );
    made.mJoint = Joint{
        .mKind = "swing_twist"_n, .mMin = -own.mTwist, .mMax = own.mTwist, .mSwing = own.mSwing, .mMaxForce = 0
    };

    // A knee bends back and an elbow forward, from what the toe points at.
    if ( own.mBend != BoneBend::Any and skeleton.mToe >= 0 )
    {
        vec3 facing = Origin( poses[static_cast<size_t>( skeleton.mToe )] ) -
                      Origin( poses[static_cast<size_t>( skeleton.mFoot )] );
        facing.y = 0.0f;
        const vec3 upper = glm::normalize( aboveTurn * above.mTip );
        const vec3 lower = glm::normalize( ownTurn * own.mTip );
        const vec3 toward = glm::length( facing ) > 0.0f
                                ? glm::normalize( facing ) * ( own.mBend == BoneBend::Knee ? -1.0f : 1.0f )
                                : vec3( 0.0f );
        const vec3 hinge = glm::cross( upper, toward );
        if ( glm::length( hinge ) > 0.1f )
        {
            // Turning about the hinge takes the lower bone from along the
            // upper toward where it bends; how far it is bent now.
            const vec3 axis = glm::normalize( hinge );
            const f32 bent = std::atan2( glm::dot( glm::cross( upper, lower ), axis ), glm::dot( upper, lower ) );
            const f32 most = Degrees( own.mBend == BoneBend::Knee ? 140.0f : 145.0f );
            made.mJoint = Joint{ .mKind = "hinge"_n,
                                 .mMin = clamp( -bent - Degrees( 3.0f ), -cPi, 0.0f ),
                                 .mMax = clamp( most - bent, 0.0f, cPi ),
                                 .mMaxForce = 0 };
            made.mOwn = AlongAxis( glm::inverse( ownTurn ) * axis );
        }
    }
    made.mAbove = Transformed( toAbove, made.mOwn );
    return made;
}

vector<mat4> JointPoses( const ModelAsset& model, OptRef<const SkinPose> skin )
{
    vector<mat4> poses( model.mJoints.size() );
    const bool posed = skin and skin->mMatrices.size() == model.mJoints.size();
    for ( size_t j = 0; j < poses.size(); ++j )
        poses[j] = posed ? skin->mMatrices[j] * glm::inverse( model.mJoints[j].mInverseBind ) : model.mJoints[j].mRest;
    return poses;
}

void PoseSkin( const ModelAsset& model, const RagdollSkeleton& skeleton, vector<mat4>& poses, SkinPose& skin )
{
    skin.mMatrices.resize( model.mJoints.size() );
    for ( size_t j = 0; j < model.mJoints.size(); ++j )
    {
        const ModelJoint& joint = model.mJoints[j];
        const i32 bone = skeleton.mBoneOf[j];
        // A joint that is not a bone's own hangs from its parent as at rest.
        const bool own = bone >= 0 and skeleton.mBones[static_cast<size_t>( bone )].mJoint == j;
        if ( not own and joint.mParent >= 0 )
        {
            const auto parent = static_cast<size_t>( joint.mParent );
            poses[j] = poses[parent] * glm::inverse( model.mJoints[parent].mRest ) * joint.mRest;
        }
        skin.mMatrices[j] = poses[j] * joint.mInverseBind;
    }
}
}
