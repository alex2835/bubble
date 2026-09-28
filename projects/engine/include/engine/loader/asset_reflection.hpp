#pragma once
#include "engine/reflection/reflection.hpp"

// Resources a component holds as Ref<T> - a model, a shader, a script, a
// sound, an animation controller - described for engine/reflection:
//
// - a file names one by its path relative to the project, null for none;
// - Lua holds the handle load_model and the rest give, nil for none;
// - the inspector picks one of what the project's loader has.
namespace bubble
{
struct Loader;

struct AssetKind
{
    // What the inspector offers besides none: the loader's, by name.
    vector<std::pair<string, entt::meta_any>> ( *mOptions )( const Loader& loader ) = nullptr;
    // A value as the inspector shows it: its name, or "None".
    string ( *mLabel )( const entt::meta_any& value ) = nullptr;
};

// Null for a type that is not a resource.
const AssetKind* FindAssetKind( const entt::meta_type& type );

// Every resource type. Once, before any component describes itself.
void ReflectAssets();

}
