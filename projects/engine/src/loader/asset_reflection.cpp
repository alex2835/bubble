#include "engine/pch/pch.hpp"
#include "engine/loader/asset_reflection.hpp"
#include "engine/loader/loader.hpp"
#include "engine/scripting/reflection_lua.hpp"
#include "engine/animation/animation_controller.hpp"
#include "engine/audio/sound.hpp"
#include "engine/renderer/model.hpp"
#include "engine/renderer/shader.hpp"
#include "engine/scripting/script.hpp"
#include "engine/types/map.hpp"
#include <nlohmann/json.hpp>

namespace bubble
{
namespace
{
hash_map<entt::id_type, AssetKind>& AssetKinds()
{
    static hash_map<entt::id_type, AssetKind> kinds;
    return kinds;
}

Loader& RequireLoader( const ReflectionContext& ctx, const string& path )
{
    if ( not ctx.mLoader )
        throw std::runtime_error( std::format( "'{}' cannot be loaded without a project", path ) );
    return *ctx.mLoader;
}

// T: the resource. Load: how the loader loads one by a path relative to the
// project. Cache: where the loader keeps what it loaded.
template <typename T, auto Load, auto Cache>
void ReflectAsset( const char* name )
{
    using Handle = Ref<T>;
    entt::meta_factory<Handle>{}.type( name );
    const entt::meta_type type = entt::resolve<Handle>();

    RegisterJsonCodec(
        type,
        []( const entt::meta_any& value, const ReflectionContext& ctx ) -> json
        {
            const Handle& resource = value.cast<const Handle&>();
            if ( not resource )
                return nullptr;
            // Without a project the path is kept as the resource has it.
            if ( not ctx.mLoader )
                return resource->mPath.generic_string();
            return ctx.mLoader->RelAbsFromProjectPath( resource->mPath ).rel.generic_string();
        },
        []( const json& j, const ReflectionContext& ctx ) -> entt::meta_any
        {
            if ( j.is_null() )
                return Handle{};
            if ( not j.is_string() )
                throw std::runtime_error( std::format( "{} is not a path", j.dump() ) );
            const string file = j.get<string>();
            return ( RequireLoader( ctx, file ).*Load )( path( file ) );
        } );

    RegisterLuaValue<Handle>();

    AssetKinds()[type.info().hash()] = AssetKind{
        .mOptions = []( const Loader& loader )
        {
            vector<std::pair<string, entt::meta_any>> options;
            for ( const auto& [file, resource] : loader.*Cache )
                options.emplace_back( file.stem().string(), entt::meta_any( resource ) );
            std::ranges::sort( options, {}, &std::pair<string, entt::meta_any>::first );
            return options;
        },
        .mLabel = []( const entt::meta_any& value ) -> string
        {
            const Handle& resource = value.cast<const Handle&>();
            return resource ? resource->mName : "None";
        },
    };
}
}

const AssetKind* FindAssetKind( const entt::meta_type& type )
{
    const auto it = AssetKinds().find( type.info().hash() );
    return it != AssetKinds().end() ? &it->second : nullptr;
}

void ReflectAssets()
{
    static const bool done = []
    {
        ReflectAsset<Model, &Loader::LoadModel, &Loader::mModels>( "model_asset" );
        ReflectAsset<Shader, &Loader::LoadShader, &Loader::mShaders>( "shader_asset" );
        ReflectAsset<Script, &Loader::LoadScript, &Loader::mScripts>( "script_asset" );
        ReflectAsset<Sound, &Loader::LoadSound, &Loader::mSounds>( "sound_asset" );
        ReflectAsset<AnimationController, &Loader::LoadAnimationController, &Loader::mControllers>( "animation_controller_asset" );
        return true;
    }();
    (void)done;
}

}
