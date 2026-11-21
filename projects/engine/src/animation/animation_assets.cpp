#include "bubble/animation/animation_graph.hpp"
#include "bubble/assets/asset_registry.hpp"
#include "bubble/data/binary.hpp"
#include "bubble/types/format.hpp"
#include "ozz_assets.hpp"
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

namespace bubble
{
namespace
{
constexpr string_view cSkeletonMagic = "BSKL";
constexpr string_view cClipsMagic = "BCLP";
constexpr u32 cVersion = 1;

// ozz's archive of a T, read from memory.
template <typename T>
expected<Ref<T>, string> TryReadArchive( string_view bytes, string_view what )
{
    ozz::io::MemoryStream stream;
    stream.Write( bytes.data(), bytes.size() );
    stream.Seek( 0, ozz::io::Stream::kSet );
    ozz::io::IArchive archive( &stream );
    if ( not archive.TestTag<T>() )
        return unexpected( format( "{} that is not ozz's", what ) );
    Ref<T> object = CreateRef<T>();
    archive >> *object;
    return object;
}

// The magic and the version, or why not.
expected<void, string> TryHeader( BinaryReader& reader, string_view magic, string_view what )
{
    const auto magicMaybe = reader.TryBytes( magic.size() );
    if ( not magicMaybe or *magicMaybe != magic )
        return unexpected( format( "not {} the importer made", what ) );
    const auto versionMaybe = reader.TryU32();
    if ( not versionMaybe )
        return unexpected( format( "{} cut short", what ) );
    if ( *versionMaybe != cVersion )
        return unexpected( format( "{} of version {}, not {}: import it again", what, *versionMaybe, cVersion ) );
    return {};
}

expected<SkeletonAsset, string> TryReadSkeleton( string_view bytes, const AssetPath& path )
{
    // Written empty: the model has no skin.
    if ( bytes.empty() )
        return unexpected( format( "{} has no skin, and so no skeleton", path.View() ) );
    BinaryReader reader( bytes );
    if ( auto headerMaybe = TryHeader( reader, cSkeletonMagic, "a skeleton" ); not headerMaybe )
        return unexpected( format( "{}: {}", path.View(), headerMaybe.error() ) );
    const auto archiveMaybe = reader.TryText();
    if ( not archiveMaybe or not reader.AtEnd() )
        return unexpected( format( "{}: a skeleton cut short", path.View() ) );
    auto skeletonMaybe = TryReadArchive<ozz::animation::Skeleton>( *archiveMaybe, "a skeleton" );
    if ( not skeletonMaybe )
        return unexpected( format( "{}: {}", path.View(), skeletonMaybe.error() ) );
    return SkeletonAsset{ .mSkeleton = std::move( *skeletonMaybe ) };
}

expected<ClipsAsset, string> TryReadClips( string_view bytes, const AssetPath& path )
{
    if ( bytes.empty() )
        return unexpected( format( "{} has no skin, and so no clips", path.View() ) );
    BinaryReader reader( bytes );
    if ( auto headerMaybe = TryHeader( reader, cClipsMagic, "clips" ); not headerMaybe )
        return unexpected( format( "{}: {}", path.View(), headerMaybe.error() ) );
    const auto countMaybe = reader.TryU32();
    if ( not countMaybe )
        return unexpected( format( "{}: clips cut short", path.View() ) );
    ClipsAsset clips;
    for ( u32 c = 0; c < *countMaybe; ++c )
    {
        const auto nameMaybe = reader.TryText();
        const auto durationMaybe = reader.TryF32();
        const auto archiveMaybe = reader.TryText();
        const auto translationMaybe = reader.TryText();
        const auto rotationMaybe = reader.TryText();
        if ( not nameMaybe or not durationMaybe or not archiveMaybe or not translationMaybe or not rotationMaybe )
            return unexpected( format( "{}: clips cut short", path.View() ) );
        auto animationMaybe = TryReadArchive<ozz::animation::Animation>( *archiveMaybe, "a clip" );
        if ( not animationMaybe )
            return unexpected( format( "{}: clip {}: {}", path.View(), *nameMaybe, animationMaybe.error() ) );
        ClipAsset clip{ .mName = string( *nameMaybe ),
                        .mDuration = *durationMaybe,
                        .mAnimation = std::move( *animationMaybe ),
                        .mRootTranslation = {},
                        .mRootRotation = {} };
        if ( not translationMaybe->empty() )
        {
            auto trackMaybe = TryReadArchive<ozz::animation::Float3Track>( *translationMaybe, "a root track" );
            auto turnMaybe = TryReadArchive<ozz::animation::QuaternionTrack>( *rotationMaybe, "a root track" );
            if ( not trackMaybe or not turnMaybe )
                return unexpected( format( "{}: clip {}: {}", path.View(), *nameMaybe,
                                           trackMaybe ? turnMaybe.error() : trackMaybe.error() ) );
            clip.mRootTranslation = std::move( *trackMaybe );
            clip.mRootRotation = std::move( *turnMaybe );
        }
        clips.mClips.push_back( std::move( clip ) );
    }
    if ( not reader.AtEnd() )
        return unexpected( format( "{}: clips with more after the last", path.View() ) );
    return clips;
}
}

opt<u32> ClipsAsset::TryFind( string_view name ) const
{
    for ( u32 c = 0; c < mClips.size(); ++c )
        if ( mClips[c].mName == name )
            return c;
    return nullopt;
}

string ClipsAsset::Names() const
{
    string names;
    for ( const ClipAsset& clip : mClips )
        names += ( names.empty() ? "" : ", " ) + clip.mName;
    return names.empty() ? "none" : names;
}

vector<string> ClipNames( string_view bytes )
{
    BinaryReader reader( bytes );
    if ( bytes.empty() or not TryHeader( reader, cClipsMagic, "clips" ) )
        return {};
    const auto countMaybe = reader.TryU32();
    if ( not countMaybe )
        return {};
    vector<string> names;
    for ( u32 c = 0; c < *countMaybe; ++c )
    {
        const auto nameMaybe = reader.TryText();
        // The rest of it: its length, its archive, its root's two tracks.
        if ( not nameMaybe or not reader.TryF32() or not reader.TryText() or not reader.TryText() or
             not reader.TryText() )
            return {};
        names.emplace_back( *nameMaybe );
    }
    return names;
}

string WriteSkeletonAsset( string_view skeletonArchive )
{
    BinaryWriter out;
    out.Bytes( cSkeletonMagic );
    out.U32( cVersion );
    out.Text( skeletonArchive );
    return out.Take();
}

string WriteClipsAsset( const vector<ImportedClip>& clips )
{
    BinaryWriter out;
    out.Bytes( cClipsMagic );
    out.U32( cVersion );
    out.U32( static_cast<u32>( clips.size() ) );
    for ( const ImportedClip& clip : clips )
    {
        out.Text( clip.mName );
        out.F32( clip.mDuration );
        out.Text( clip.mAnimation );
        out.Text( clip.mRootTranslation );
        out.Text( clip.mRootRotation );
    }
    return out.Take();
}

void RegisterAnimationLoaders( AssetRegistry& registry )
{
    registry.RegisterLoader<SkeletonAsset>( cSkeletonKind, &TryReadSkeleton );
    registry.RegisterLoader<ClipsAsset>( cClipsKind, &TryReadClips );
    registry.RegisterLoader<AnimationGraph>(
        cAnimationGraphKind, []( string_view bytes, const AssetPath& path ) -> expected<AnimationGraph, string> {
            auto graphMaybe = TryParseAnimationGraph( bytes );
            if ( not graphMaybe )
                return unexpected( format( "{}:{}", path.View(), graphMaybe.error() ) );
            return std::move( *graphMaybe );
        } );
}
}
