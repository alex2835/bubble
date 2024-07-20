#pragma once
#include "engine/serialization/loader_serialization.hpp"
#include "engine/window/window.hpp"
#include <nlohmann/json.hpp>
#include <future>

namespace bubble
{
void to_json( json& j, const Loader& loader )
{
    auto& jsonTexture = j["textures"];
    for ( const auto& [path, _] : loader.mTextures )
        jsonTexture.push_back( path );

    auto& jsonShaders = j["shaders"];
    for ( const auto& [path, _] : loader.mShaders )
        jsonShaders.push_back( path );

    auto& jsonModels = j["models"];
    for ( const auto& [path, _] : loader.mModels )
        jsonModels.push_back( path );

    auto& jsonSkyboxes = j["skyboxes"];
    for ( const auto& [path, _] : loader.mSkyboxes )
        jsonSkyboxes.push_back( path );
}

void from_json( const json& j, Loader& loader )
{
    //if ( j.contains( "textures" ) && !j["textures"].is_null() )
    //    for ( const auto& texturePath : j["textures"] )
    //        loader.LoadTexture2D( texturePath );
    
    if ( j.contains( "textures" ) && !j["textures"].is_null() )
        loader.LoadTextures2D( j["textures"] );

    if ( j.contains( "models" ) && !j["models"].is_null() )
        loader.LoadModels( j["models"] );

    if ( j.contains( "shaders" ) && !j["shaders"].is_null() )
        for ( const auto& shaderPath : j["shaders"] )
            loader.LoadShader( shaderPath );

    if ( j.contains( "skyboxes" ) && !j["skyboxes"].is_null() )
        for ( const auto& skyboxPath : j["skyboxes"] )
            loader.LoadSkybox( skyboxPath );
}

}