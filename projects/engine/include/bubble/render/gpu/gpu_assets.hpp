#pragma once
#include "bubble/assets/asset_registry.hpp"
#include "bubble/assets/font_asset.hpp"
#include "bubble/assets/model_asset.hpp"
#include "bubble/assets/texture_asset.hpp"
#include "bubble/render/gpu/gpu_model.hpp"
#include "bubble/render/gpu/gpu_texture.hpp"

namespace bubble
{
// The GPU's copies of the registry's models and textures, by id: made at
// the first ask, made again at the first ask after a reload, let go by
// Collect once the registry has let go of the asset. One a process, beside
// the registry.
class GpuAssets
{
public:
    // The Gpu and the registry outlive it.
    GpuAssets( const Gpu& gpu, const AssetRegistry& assets );
    GpuAssets( const GpuAssets& ) = delete;
    GpuAssets& operator=( const GpuAssets& ) = delete;

    // The copy of an asset that is Ready; nothing while it loads, or when it
    // failed. Good until the next ask after a reload, or Collect.
    OptRef<const GpuModel> TryModel( const AssetRef<ModelAsset>& modelRef );
    OptRef<const GpuTexture> TryTexture( const AssetRef<TextureAsset>& textureRef );
    // A font's atlas.
    OptRef<const GpuTexture> TryFont( const AssetRef<FontAsset>& fontRef );

    // Lets go of the copies of assets no longer in memory: once a frame, or
    // now and then.
    void Collect();
    // How many copies it keeps.
    size_t Count() const { return mModels.size() + mTextures.size() + mFonts.size(); }

private:
    // A copy, and the asset's version it was made of.
    template <typename T>
    struct Copy
    {
        u32 mVersion = 0;
        T mGpu;
    };

    const Gpu& mGpu;
    const AssetRegistry& mAssets;
    hmap<AssetId, Copy<GpuModel>> mModels;
    hmap<AssetId, Copy<GpuTexture>> mTextures;
    hmap<AssetId, Copy<GpuTexture>> mFonts;
};
}
