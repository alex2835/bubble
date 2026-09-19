#include "engine/pch/pch.hpp"
#include "engine/loader/loader.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

namespace bubble
{
Ref<AnimationController> LoadAnimationController( const path& controllerPath )
{
    std::ifstream file( controllerPath );
    if ( not file.is_open() )
    {
        LogError( "Failed to open animation controller: {}", controllerPath.string() );
        return nullptr;
    }
    try
    {
        const json j = json::parse( file );
        return CreateRef<AnimationController>( AnimationController::FromJson( j, controllerPath ) );
    }
    catch ( const std::exception& e )
    {
        // A parse error from nlohmann, or a validation error from FromJson
        // that already names the file.
        LogError( "Animation controller {}: {}", controllerPath.string(), e.what() );
        return nullptr;
    }
}


Ref<AnimationController> Loader::LoadAnimationController( const path& controllerPath )
{
    auto [relPath, absPath] = RelAbsFromProjectPath( controllerPath );

    auto iter = mControllers.find( relPath );
    if ( iter != mControllers.end() )
        return iter->second;

    auto controller = bubble::LoadAnimationController( absPath );
    if ( not controller )
        return nullptr;

    mControllers.emplace( relPath, controller );
    mResourcesGeneration = NextResourcesGeneration();
    return controller;
}

}
