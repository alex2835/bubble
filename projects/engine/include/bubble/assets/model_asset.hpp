#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/math.hpp"
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
// coarser one.
struct ModelMesh
{
    string mMaterial;
    vector<ModelVertex> mVertices;
    vector<ModelLod> mLods;
};

// A model as the importer made it: its meshes, their nodes' transforms
// already applied, and the box around them all.
struct ModelAsset
{
    vec3 mMin{ 0.0f };
    vec3 mMax{ 0.0f };
    vector<ModelMesh> mMeshes;
};

constexpr string_view cModelKind = "model";

// The bytes the importer writes and the loader reads: "BMDL", the version,
// the box, then each mesh - its material, its vertices, its levels -
// little-endian.
string WriteModelAsset( const ModelAsset& model );
// The error says what is wrong: cut short, another version, an index past
// the vertices.
expected<ModelAsset, string> TryReadModelAsset( string_view bytes );

// Teaches the registry models: <id>.model.
void RegisterModelLoader( AssetRegistry& registry );
}
