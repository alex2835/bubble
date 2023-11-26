#pragma once
#include <unordered_map>
#include <exception>
#include <tuple>
#include "utils/imexp.hpp"
#include "utils/pointers.hpp"
#include "log/log.hpp"
#include "renderer/texture.hpp"
#include "renderer/shader.hpp"
#include "renderer/model.hpp"
#include "renderer/skybox.hpp"

namespace bubble
{
struct BUBBLE_ENGINE_EXPORT Loader
{
    // Storage
    std::unordered_map<std::string, Ref<Texture2D>> mTextures;
    std::unordered_map<std::string, Ref<Model>>     mModels;
    std::unordered_map<std::string, Ref<Shader>>    mShaders;
    std::unordered_map<std::string, Ref<Skybox>>    mSkyboxes;
    std::unordered_map<std::string, Ref<Texture2D>> mSkyspheres;


    Ref<Shader> LoadShader( const std::string& path );
    Ref<Shader> LoadShader( const std::string& name,
                            const std::string& vertex,
                            const std::string& fragment,
                            const std::string& geometry = std::string() );


    std::tuple<Scope<uint8_t[]>, Texture2DSpecification>
    OpenRawImage( const std::string& path, Texture2DSpecification spec = {} );
    
    Ref<Texture2D> LoadAndCacheTexture2D( std::string path, const Texture2DSpecification& spec = {} );

    Ref<Model> LoadAndCacheModel( std::string path );

private:
    void ParseShaders( const std::string& path,
                       std::string& vertex,
                       std::string& fragment,
                       std::string& geometry );

    void CompileShaders( Shader& shader,
                         const std::string& vertex_source,
                         const std::string& fragment_source,
                         const std::string& geometry_source );
};

}