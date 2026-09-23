#pragma once
#include "engine/log/log.hpp"
#include "engine/types/string.hpp"
#include "engine/types/pointer.hpp"
#include "engine/types/map.hpp"
#include "engine/types/array.hpp"
#include "engine/types/utility.hpp"
#include "engine/utils/filesystem.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/shader.hpp"
#include "engine/renderer/model.hpp"
#include "engine/scripting/script.hpp"
#include "engine/audio/sound.hpp"
#include "engine/animation/animation_controller.hpp"
#include "engine/loader/shader_module_loader.hpp"
#include <functional>

namespace Assimp { class Importer; }
namespace ozz::animation { class Skeleton; }
namespace ozz::animation::offline { struct RawAnimation; }
struct aiScene;
struct aiMesh;

namespace bubble
{
// error resources
constexpr string_view ERROR_MODEL = "./resources/models/error.obj"sv;
constexpr string_view ERROR_TEXTURE = "./resources/images/utils/error.jpg"sv;
// scene icons textures
constexpr string_view SCENE_CAMERA_TEXTURE = "./resources/images/scene/camera.png"sv;
constexpr string_view SCENE_POINT_LIGHT_TEXTURE = "./resources/images/scene/pointlight.png"sv;
constexpr string_view SCENE_SPOT_LIGHT_TEXTURE = "./resources/images/scene/spotlight.png"sv;
constexpr string_view SCENE_DIR_LIGHT_TEXTURE = "./resources/images/scene/dirlight.png"sv;
constexpr string_view SCENE_AUDIO_TEXTURE = "./resources/images/scene/audio.png"sv;
// shader paths
constexpr string_view ENTITY_PICKING_SHADER = "./resources/shaders/object_picking"sv; // Object id shader to select entity from screen
constexpr string_view ENTITY_PICKING_BILLBOARD_SHADER = "./resources/shaders/object_picking_billboard"sv;
constexpr string_view WHITE_SHADER = "./resources/shaders/white"sv;
constexpr string_view PHONG_SHADER = "./resources/shaders/phong"sv;
constexpr string_view ONLY_DIFFUSE_SHADER = "./resources/shaders/only_diffuse"sv;
constexpr string_view BILBOARD_SHADER = "./resources/shaders/billboard"sv;
// Used when a shader supplies a fragment stage and no vertex stage of its own.
// Most shaders differ only in how they shade a fragment, and needing a copy of
// this file per shader is what makes every new shader start as a duplicate.


// Loading is split in two: Open* reads and decodes on any thread and returns
// plain memory, Load* turns that into GPU objects.
//
// The split dates from OpenGL, whose context was bound to one thread. WebGPU's
// device and queue are thread safe, so it is no longer forced - but it is kept,
// because the browser build has WebGPU only on the main thread, and because the
// Loader's caches are not synchronized. Decoding is the expensive part anyway,
// and that is what runs in parallel.

struct TextureData
{
    Scope<u8[]> mData;
    Texture2DSpecification mSpec;
    path mPath;
};

// The skeleton a skinned model binds to and the clips that came with it,
// already built into ozz's runtime form. Empty for a static model.
struct SkeletonData
{
    Ref<Skeleton> mSkeleton;
    vector<Ref<AnimationClip>> mClips;
};

struct ModelData
{
    Scope<Assimp::Importer> mImporter;
    // Every texture the model's materials reference, decoded, keyed by the
    // absolute path the material resolves to. LoadModel uploads from here
    // rather than reading the files again.
    map<path, TextureData> mTexturesData;
    // Built alongside the decode, for the same reason the textures are: it is
    // the expensive part, and it needs no GPU.
    std::optional<SkeletonData> mSkeleton;
    path mPath;
};

// Nullopt for a scene with no bones. Skeleton joints are the bone nodes and
// their ancestors; every clip in the scene is imported against that skeleton.
std::optional<SkeletonData> ImportSkeleton( const aiScene* scene, const path& modelPath );
// Fills the mesh's joint indices and weights, remapped from assimp's per mesh
// bone indices to the skeleton's. Leaves them empty for a mesh without bones.
void ImportMeshSkin( const aiMesh* mesh, const Skeleton& skeleton, const path& modelPath, VertexBufferData& vertices );
// A clip retargeted from a rig of another size can come with that size baked
// into one joint: a uniform scale held for the whole clip, and the joint's
// translation grown to match so the feet still reach the floor. Played beside
// the model's other clips, the character grows and shrinks as they blend.
// For every joint whose scale is uniform, never changes over the clip and is
// not its rest scale, this puts the rest scale back and divides the joint's
// translation by the same factor: the same motion, at the model's size.
// Scale that changes over the clip is animation and is left alone. Returns
// the joints it changed.
vector<string> NormalizeConstantScale( ozz::animation::offline::RawAnimation& raw,
                                       const ozz::animation::Skeleton& skeleton );

std::optional<TextureData> OpenTexture( const path& path );
// An encoded image already in memory - a texture embedded in a glTF binary.
// The name only identifies it; nothing is read from that path.
std::optional<TextureData> OpenTexture( const u8* bytes, u64 size, const path& name );
Ref<Texture2D> LoadTexture2D( const path& path );
Ref<Texture2D> LoadTexture2D( const TextureData& textureData );

// Turns decoded texture data into the GPU texture the model will bind. The
// caller decides where that texture lives - Loader's cache, so meshes and
// models sharing a file share one texture, or a throwaway map for a model
// loaded outside any Loader.
using TextureUploader = std::function<Ref<Texture2D>( const TextureData& )>;

std::optional<ModelData> OpenModel( const path& modelPath );
Ref<Model> LoadModel( const path& path );
Ref<Model> LoadModel( const ModelData& modelData, const TextureUploader& uploadTexture );

Ref<Shader> LoadShader( const path& path );

Ref<Script> LoadScript( const path& path );

Ref<Sound> LoadSound( const path& path );

// A .anim JSON file - see animation_controller.hpp. Null, with the error
// logged, for one that does not parse.
Ref<AnimationController> LoadAnimationController( const path& path );



struct Loader
{
    Loader() = default;
    Ref<Script> LoadScript( const path& path );
    Ref<Sound> LoadSound( const path& path );
    Ref<AnimationController> LoadAnimationController( const path& path );
    Ref<Texture2D> LoadTexture2D( const path& path );
    void LoadTextures2D( const vector<path>& paths );
    // The TextureUploader handed to LoadModel. Returns the texture already
    // cached for that path from either map, or uploads into mModelTextures.
    Ref<Texture2D> UploadTexture2D( const TextureData& textureData );
    Ref<Shader> LoadShader( path path );
    Ref<Model> LoadModel( const path& path );
    void LoadModels( const vector<path>& paths );

    
    struct ProjectPath
    {
        path rel;
        path abs;
    };
    ProjectPath RelAbsFromProjectPath( const path& resourcePath ) const;

    // Values are unique across every Loader in the process, never reused and
    // never reset, so replacing the whole Loader on a project switch always
    // reads as a change - even if the new project happens to load the same
    // number of resources before anyone looks.
    static u64 NextResourcesGeneration();


public:
    path mProjectRootDir;
    hash_map<path, Ref<Texture2D>> mTextures;
    // Textures reached only through a model's materials. Kept apart from
    // mTextures so they are not serialized into the project's texture list
    // and offered as project textures - the model brings them back on its
    // own. A path is in at most one of the two maps; loading one of these
    // explicitly through LoadTexture2D moves it to mTextures.
    hash_map<path, Ref<Texture2D>> mModelTextures;
    hash_map<path, Ref<Model>> mModels;
    hash_map<path, Ref<Shader>> mShaders;
    hash_map<path, Ref<Script>> mScripts;
    hash_map<path, Ref<Sound>> mSounds;
    hash_map<path, Ref<AnimationController>> mControllers;

    // Stamped whenever mShaders or mScripts gains an entry, so an observer can
    // tell in one comparison whether its view of them is out of date. Erasing
    // from those maps directly does not go through the Loader, and so does not
    // stamp it.
    u64 mResourcesGeneration = NextResourcesGeneration();
};

}