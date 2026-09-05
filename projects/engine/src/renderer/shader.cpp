#include "engine/pch/pch.hpp"
#include "engine/log/log.hpp"
#include "engine/renderer/shader.hpp"
#include "engine/renderer/gpu_context.hpp"

namespace bubble
{

// Via Swap so there is exactly one list of members to keep up to date. The
// hand-written member list this replaced moved three of them and left mPath,
// mModules and mUniformDescriptors behind - a moved-from shader kept its path,
// and the moved-to one had no material and no uniforms.
Shader::Shader( Shader&& other ) noexcept
{
    Swap( other );
}

Shader& Shader::operator=( Shader&& other ) noexcept
{
    if ( this != &other )
        Swap( other );
    return *this;
}

void Shader::Swap( Shader& other ) noexcept
{
    std::swap( mModule, other.mModule );
    std::swap( mName, other.mName );
    std::swap( mPath, other.mPath );
    std::swap( mModules, other.mModules );
    // Missing here before, and the hot reloader swaps a freshly compiled shader
    // into the live one - so a reloaded shader kept the old descriptor set and
    // a uniform added to the file never reached the inspector.
    std::swap( mUniformDescriptors, other.mUniformDescriptors );
    std::swap( mUniformDefaults, other.mUniformDefaults );
    // Pipelines belong to the module they were built from, so they travel with
    // it rather than being cleared.
    std::swap( mPipelines, other.mPipelines );
}

wgpu::RenderPipeline Shader::GetPipeline( const PipelineKey& key, const VertexLayout& layout )
{
    BUBBLE_ASSERT( mModule, "GetPipeline on a shader that failed to compile" );

    for ( const auto& [cachedKey, pipeline] : mPipelines )
    {
        if ( cachedKey == key )
            return *pipeline;
    }

    auto pipeline = CreateRenderPipeline( *mModule, key, layout, mName );
    if ( not pipeline )
    {
        LogError( "Shader {}: could not create a render pipeline", mName );
        return {};
    }

    mPipelines.emplace_back( key, std::move( pipeline ) );
    return *mPipelines.back().second;
}

void Shader::ClearPipelines()
{
    mPipelines.clear();
}

}
