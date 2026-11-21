#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"

namespace bubble
{
class AssetRegistry;

// What a model's importer makes beside the model when it has a skin, under
// the model's own id: its skeleton and its clips, in ozz's archives. The
// types they load as are the animation module's own (src/animation): ozz
// is seen nowhere else in the engine.
constexpr string_view cSkeletonKind = "skeleton";
constexpr string_view cClipsKind = "clips";

// One clip as the importer hands it over: its name, its length in seconds,
// and ozz's archive of it, sampled against the skeleton of the same source.
// With root motion taken out of it, the clip plays on the spot, and two
// tracks - ozz's archives too - hold where its root went, in the model's
// space: across the ground, and turning about the up axis. Empty without.
struct ImportedClip
{
    string mName;
    f32 mDuration = 0;
    string mAnimation;
    string mRootTranslation = {};
    string mRootRotation = {};
};

// "BSKL", the version, then ozz's archive of the skeleton.
string WriteSkeletonAsset( string_view skeletonArchive );
// "BCLP", the version, then each clip: its name, its length, its archive,
// its root's two tracks.
string WriteClipsAsset( const vector<ImportedClip>& clips );

// The clips' names in a clips asset's bytes, in their order: what the
// editor offers a state to play. None when the bytes do not read.
vector<string> ClipNames( string_view bytes );

// Teaches the registry skeletons, clips and animation graphs: <id>.skeleton,
// <id>.clips, <id>.animation_graph.
void RegisterAnimationLoaders( AssetRegistry& registry );
}
