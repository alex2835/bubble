#pragma once
#include "engine/serialization/loader_serialization.hpp"
#include <nlohmann/json.hpp>

namespace bubble
{
void to_json( json& j, const Loader& loader )
{
    auto& jsonTexture = j["textures"];
    for ( const auto& [path, _] : loader.mTextures )
        jsonTexture.push_back( path );

    auto& jsonModels = j["models"];
    for ( const auto& [path, _] : loader.mModels )
        jsonModels.push_back( path );

    auto& jsonShaders = j["shaders"];
    for ( const auto& [path, _] : loader.mShaders )
        jsonShaders.push_back( path );

    auto& jsonSkyboxes = j["skyboxes"];
    for ( const auto& [path, _] : loader.mSkyboxes )
        jsonSkyboxes.push_back( path );
}

void from_json( const json& j, Loader& loader )
{
    if ( j.contains( "textures" ) && !j["textures"].is_null() )
        for ( const auto& texturePath : j["textures"] )
            loader.LoadTexture2D( texturePath );

    if ( j.contains( "models" ) && !j["models"].is_null() )
        for ( const auto& modelPath : j["models"] )
            loader.LoadModel( modelPath );

    if ( j.contains( "shaders" ) && !j["shaders"].is_null() )
        for ( const auto& shaderPath : j["shaders"] )
            loader.LoadShader( shaderPath );

    //if ( j.contains( "shaders" ) )
    //    for ( const auto& skyboxePath : j["skyboxes"] )
    //        loader.LoadSkybox( skyboxePath );
}

}