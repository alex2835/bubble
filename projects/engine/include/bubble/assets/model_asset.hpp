#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/math.hpp"
#include "bubble/types/matrix.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
class AssetRegistry;

// A vertex as the importer writes it. The layout is fixed for now; what a
// renderer wants - quantized, tangents, more texture coordinates - comes as
// a new version of it.
struct ModelVertex
{
    vec3 mPosition{ 0.0f };
    vec3 mNormal{ 0.0f };
    // From the image's top left, as textures are stored.
    vec2 mUv{ 0.0f };
};

// Which joints move a vertex of a skinned mesh, and by how much: four at
// most, by their places in the model's mJoints, the weights adding up to 1.
struct ModelSkinVertex
{
    array<u16, 4> mJoints{};
    vec4 mWeights{ 0.0f };
};

// One level of detail of a mesh: its triangles, as indices into the mesh's
// vertices, which every level shares. `mError` is how far it strays from
// the full mesh, in the model's units - what choosing a level by its size on
// screen weighs; 0 for the full mesh.
struct ModelLod
{
    f32 mError = 0;
    vector<u32> mIndices;
};

// What one material covers: the full mesh first among its levels, then each
// coarser one. A skinned mesh has a skin vertex for each of its vertices,
// which lie where they were when the skin was bound; one that is not has
// none, and its vertices lie in the model's space.
struct ModelMesh
{
    string mMaterial;
    vector<ModelVertex> mVertices;
    vector<ModelSkinVertex> mSkin = {};
    vector<ModelLod> mLods;
};

// A joint of the skeleton a model's skin is bound to, in the skeleton's
// order: its name; where it is at rest, in the model's space; and the
// inverse of where it was when the skin was bound, which takes a vertex
// into the joint's space - the identity for a joint that moves no vertex.
// A vertex is drawn by its joints' pose times their inverse binds; at rest,
// by mRest times mInverseBind.
struct ModelJoint
{
    string mName;
    // At rest, in the model's space.
    mat4 mRest{ 1.0f };
    mat4 mInverseBind{ 1.0f };
    // The joint it hangs from, always before it; -1 for a root.
    i32 mParent = -1;
};

// A model as the importer made it: its meshes - those not skinned with
// their nodes' transforms already applied -, the box around them all at
// rest, and the joints of its skin, none for a model without.
struct ModelAsset
{
    vec3 mMin{ 0.0f };
    vec3 mMax{ 0.0f };
    vector<ModelMesh> mMeshes;
    vector<ModelJoint> mJoints = {};
};

// What an entity's skinned model is drawn with this frame, a matrix a joint
// of its model: the joint's pose times its inverse bind, in the model's
// space. Runtime: the animation module writes it, the render module reads
// it; a skinned model without one, or with one of another length, is drawn
// at rest.
struct SkinPose
{
    vector<mat4> mMatrices;
};

constexpr string_view cModelKind = "model";

// The bytes the importer writes and the loader reads: "BMDL", the version,
// the box, then each mesh - its material, its vertices, its skin, its
// levels -, then the joints, little-endian.
string WriteModelAsset( const ModelAsset& model );
// The error says what is wrong: cut short, another version, an index past
// the vertices, a skin of another length than the vertices, a joint past
// the joints.
expected<ModelAsset, string> TryReadModelAsset( string_view bytes );

// Teaches the registry models: <id>.model.
void RegisterModelLoader( AssetRegistry& registry );
}
