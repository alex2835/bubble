#include "bubble/render/gpu/gpu_assets.hpp"

namespace bubble
{
namespace
{
// The copy of a Ready asset, made by `make` when there is none yet or the
// asset was reloaded since.
template <typename Copies, typename Asset, typename Make>
auto TryCopyOf( Copies& copies, const AssetRef<Asset>& assetRef, const Make& make )
    -> OptRef<const decltype( Copies::mapped_type::mGpu )>
{
    const auto assetMaybe = assetRef.TryGet();
    if ( not assetMaybe )
        return nullopt;
    const AssetEntryBase& entry = assetRef.Entry();
    auto found = copies.find( entry.Id() );
    if ( found == copies.end() )
        found = copies
                    .emplace( entry.Id(), typename Copies::mapped_type{ entry.Version(),
                                                                        make( *assetMaybe, entry.Path().View() ) } )
                    .first;
    else if ( found->second.mVersion != entry.Version() )
        found->second = typename Copies::mapped_type{ entry.Version(), make( *assetMaybe, entry.Path().View() ) };
    return found->second.mGpu;
}
}

GpuAssets::GpuAssets( const Gpu& gpu, const AssetRegistry& assets ) : mGpu( gpu ), mAssets( assets )
{
}

OptRef<const GpuModel> GpuAssets::TryModel( const AssetRef<ModelAsset>& modelRef )
{
    return TryCopyOf( mModels, modelRef,
                      [this]( const ModelAsset& model, string_view label ) { return GpuModel( mGpu, model, label ); } );
}

OptRef<const GpuTexture> GpuAssets::TryTexture( const AssetRef<TextureAsset>& textureRef )
{
    return TryCopyOf( mTextures, textureRef, [this]( const TextureAsset& texture, string_view label ) {
        return GpuTexture( mGpu, texture, label );
    } );
}

OptRef<const GpuTexture> GpuAssets::TryFont( const AssetRef<FontAsset>& fontRef )
{
    return TryCopyOf( mFonts, fontRef, [this]( const FontAsset& font, string_view label ) {
        return GpuTexture( mGpu, font.mAtlas, label );
    } );
}

void GpuAssets::Collect()
{
    erase_if( mModels, [this]( const auto& copy ) { return not mAssets.TryFind<ModelAsset>( copy.first ); } );
    erase_if( mTextures, [this]( const auto& copy ) { return not mAssets.TryFind<TextureAsset>( copy.first ); } );
    erase_if( mFonts, [this]( const auto& copy ) { return not mAssets.TryFind<FontAsset>( copy.first ); } );
}
}
