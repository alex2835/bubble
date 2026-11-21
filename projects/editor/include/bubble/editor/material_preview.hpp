#pragma once
#include "bubble/assets/material_asset.hpp"
#include "bubble/assets/model_asset.hpp"
#include "bubble/assets/shader_asset.hpp"
#include "bubble/editing/data_document.hpp"
#include "bubble/editor/editor_camera.hpp"
#include "bubble/editor/editor_project.hpp"
#include "bubble/render/material_layout.hpp"
#include "bubble/world/world.hpp"

namespace bubble
{
// What shows a material's document: a world of its own - a sphere wearing
// the material as the document has it, saved or not, under a sun - and the
// camera on it. The sphere and the material are the registry's, set from
// memory under ids of their own (AssetRegistry::Set); kept with the document
// as its view.
//
// It also knows what the inspector shows: the material's bases and the
// parameters its shader declares - the root base's shader, or the engine's
// standard material -, followed as they load and change.
class MaterialPreview final : public DocumentView
{
public:
    MaterialPreview( EditorProject& project, const DataDocument& document );

    World& Get() { return *mWorld; }
    EditorCamera& Camera() { return mCamera; }
    // Each frame: the material made again from the document when it
    // changed - one that does not read leaves the last that did -, the bases
    // and the shader looked at again.
    void Refresh( const DataDocument& document );

    // The document's material as it reads now; nothing when it does not.
    OptRef<const MaterialAsset> TryMaterial() const;
    // Its bases, nearest first, as far as they have loaded.
    span<const MaterialAsset> Bases() const { return mBases; }
    // What its shader declares; nothing while the bases or the shader load,
    // or when one of them fails - Error() says why.
    OptRef<const MaterialLayout> TryLayout() const;
    // Why the material does not read, or its bases or its shader do not;
    // empty when nothing is wrong.
    const string& Error() const { return mError; }

private:
    // The bases and the shader as the registry has them: the layout
    // reflected again when any of them, or the document, changed.
    void Resolve( const DataDocument& document );

    EditorProject& mProject;
    AssetId mSphereId = AssetId::Generate();
    AssetId mMaterialId = AssetId::Generate();
    AssetRef<ModelAsset> mSphere;
    AssetRef<MaterialAsset> mMaterial;
    u64 mShownVersion = ~u64( 0 );
    opt<MaterialAsset> mReadMaybe;
    string mReadError;
    // What the layout was made from: the document's version, each base's
    // and the shader's id, version and state.
    string mResolved;
    vector<AssetRef<MaterialAsset>> mBaseRefs;
    vector<MaterialAsset> mBases;
    AssetRef<ShaderAsset> mShader;
    opt<MaterialLayout> mLayoutMaybe;
    string mError;
    EditorCamera mCamera;
    // Last: its modules hold the refs above.
    Scope<World> mWorld;
};

// The material document's preview, made the first time it is asked for.
MaterialPreview& PreviewOf( EditorProject& project, DataDocument& document );
}
