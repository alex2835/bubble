// Materials on the GPU: the renderer's own, not a header of its interface.
#pragma once
#include "bubble/assets/material_asset.hpp"
#include "bubble/assets/shader_asset.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/render/gpu/gpu_assets.hpp"
#include "bubble/render/material_layout.hpp"

namespace bubble
{
constexpr WGPUTextureFormat cHdrFormat = WGPUTextureFormat_RGBA16Float;
constexpr WGPUTextureFormat cDepthFormat = WGPUTextureFormat_Depth32Float;

// A material's shader on the GPU: its WGSL with the engine's lit.wgsl after
// it, what it declares, the layout of its bind group (group 2), and a
// pipeline for each way a material draws - opaque or transparent, one side
// or two.
class MaterialShader
{
public:
    enum class State : u8
    {
        // The GPU has not said whether it compiles.
        Waiting,
        Ready,
        Failed,
    };

    // Built at once, inside a GPU check. A user's waits for the check's
    // answer before it is drawn with: an invalid pipeline would lose the
    // whole frame. The engine's own is trusted meanwhile.
    MaterialShader( Gpu& gpu,
                    string_view source,
                    string_view label,
                    bool trusted,
                    WGPUBindGroupLayout frame,
                    WGPUBindGroupLayout objects );

    // Takes the check's answer when it has come.
    State Poll();
    // Why it failed: its contract broken, or the GPU's message, the line in
    // its file.
    const string& Error() const { return mError; }
    const MaterialLayout& Layout() const { return mLayout; }
    WGPUBindGroupLayout GroupLayout() const { return mGroupLayout.Get(); }
    WGPURenderPipeline Pipeline( bool transparent, bool doubleSided ) const;

private:
    State mState = State::Waiting;
    string mError;
    MaterialLayout mLayout;
    GpuShaderHandle mModule;
    GpuBindGroupLayoutHandle mGroupLayout;
    array<GpuRenderPipelineHandle, 4> mPipelines;
    opt<GpuCheck> mCheck;
};

// A material as a draw takes it: a pipeline and its bind group.
struct GpuMaterial
{
    // Its shader's, which outlives it: a shader built again builds its
    // materials again.
    WGPURenderPipeline mPipeline = nullptr;
    bool mTransparent = false;
    bool mDoubleSided = false;
    GpuBuffer mUniforms;
    GpuBindGroupHandle mGroup;
};

// The renderer's materials: each built from its chain of bases, its shader
// and its textures, which it loads, and built again when any of them
// changes or a texture comes.
class GpuMaterials
{
public:
    // Everything given outlives it.
    GpuMaterials( Gpu& gpu,
                  AssetRegistry& assets,
                  GpuAssets& gpuAssets,
                  WGPUBindGroupLayout frame,
                  WGPUBindGroupLayout objects );
    ~GpuMaterials();
    GpuMaterials( const GpuMaterials& ) = delete;
    GpuMaterials& operator=( const GpuMaterials& ) = delete;

    // Once a frame, before For: what is asked again is looked at again.
    void BeginFrame();
    // What a mesh with `materialRef` draws with: the material when it and
    // everything it needs have come; the standard material while they load,
    // and for an empty ref; the placeholder - glowing magenta - when it
    // cannot be drawn, the reason logged once. Good for the frame.
    const GpuMaterial& For( const AssetRef<MaterialAsset>& materialRef );

private:
    // An asset a material was built from, as it was then.
    struct Seen
    {
        AssetId mId;
        u32 mVersion = 0;
        bool mReady = false;
        friend bool operator==( const Seen&, const Seen& ) = default;
    };

    enum class Use : u8
    {
        Own,
        Standard,
        Placeholder,
    };

    struct Entry
    {
        u64 mFrame = ~0ull;
        Use mUse = Use::Standard;
        opt<GpuMaterial> mBuilt;
        vector<Seen> mSeen;
        string mLogged;
        // Held, so what it needs stays loaded - and loads.
        vector<AssetRef<MaterialAsset>> mChain;
        AssetRef<ShaderAsset> mShader;
        vector<AssetRef<TextureAsset>> mTextures;
    };

    struct ShaderCopy
    {
        u32 mVersion = 0;
        // Grows with every shader built: a material built from another
        // generation is built again.
        u32 mGeneration = 0;
        Scope<MaterialShader> mShader;
    };

    void Resolve( Entry& entry, const AssetRef<MaterialAsset>& materialRef );
    void Fail( Entry& entry, const string& reason );
    ShaderCopy& ShaderFor( const AssetRef<ShaderAsset>& shaderRef );
    GpuMaterial Build( const MaterialShader& shader,
                       const vector<OptRef<const MaterialAsset>>& chain,
                       const vector<WGPUTextureView>& textures,
                       bool transparent,
                       bool doubleSided );
    WGPUTextureView DefaultTexture( TextureDefault kind ) const;

    Gpu& mGpu;
    AssetRegistry& mAssets;
    GpuAssets& mGpuAssets;
    WGPUBindGroupLayout mFrameLayout;
    WGPUBindGroupLayout mObjectsLayout;
    u64 mFrame = 0;
    u32 mGenerations = 0;

    GpuSamplerHandle mSampler;
    GpuTexture mWhite;
    GpuTexture mBlack;
    GpuTexture mFlatNormal;
    Scope<MaterialShader> mStandardShader;
    GpuMaterial mStandard;
    GpuMaterial mPlaceholder;
    hmap<AssetId, ShaderCopy> mShaders;
    hmap<AssetId, Entry> mEntries;
};
}
