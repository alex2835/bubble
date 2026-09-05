#include "engine/pch/pch.hpp"
#include "engine/loader/loader.hpp"
#include "engine/types/string.hpp"
#include "engine/loader/shader_module_loader.hpp"
#include "engine/renderer/gpu_context.hpp"
#include "engine/log/log.hpp"

namespace bubble
{
namespace
{

// Renders the expanded source with line numbers next to a compile error.
//
// The include expander emits no #line directives, so the line numbers a
// compiler reports refer to the expanded text and not to any file on disk.
// Printing that text is the only way to make them mean anything. This replaced
// shader_loader_error_handling.cpp, which attributed errors to a vertex,
// fragment or geometry stage - WGSL compiles one module holding every entry
// point, so there are no stages to attribute to.
void LogShaderSource( string_view name, string_view source )
{
    string numbered;
    numbered.reserve( source.size() + source.size() / 8 );

    u32 line = 1;
    u64 start = 0;
    while ( start <= source.size() )
    {
        const u64 end = source.find( '\n', start );
        const string_view text = source.substr( start, end == string::npos ? string::npos : end - start );
        numbered += std::format( "{:>4} | {}\n", line, text );
        if ( end == string::npos )
            break;
        start = end + 1;
        line++;
    }
    LogError( "Expanded source of {}:\n{}", name, numbered );
}

// Compilation is asynchronous in WebGPU, and a failed module still returns a
// non-null handle - the error arrives through the device error scope. Wrapping
// creation in a scope and draining it keeps the synchronous shape the loader
// and the hot reloader are written around.
wgpu::raii::ShaderModule CompileWGSL( string_view name, string_view source, bool& outOk )
{
    outOk = true;

    wgpu::ShaderSourceWGSL wgslDesc = wgpu::Default;
    wgslDesc.code = wgpu::StringView( source );

    wgpu::ShaderModuleDescriptor desc = wgpu::Default;
    desc.nextInChain = &wgslDesc.chain;
    desc.label = wgpu::StringView( name );

    Gpu().Device().pushErrorScope( wgpu::ErrorFilter::Validation );
    wgpu::raii::ShaderModule module( Gpu().Device().createShaderModule( desc ) );

    bool done = false;
    bool failed = false;
    string message;

    wgpu::PopErrorScopeCallbackInfo callbackInfo = wgpu::Default;
    callbackInfo.mode = wgpu::CallbackMode::AllowProcessEvents;
    callbackInfo.callback = []( WGPUPopErrorScopeStatus,
                                WGPUErrorType type,
                                WGPUStringView msg,
                                void* userdata1,
                                void* userdata2 )
    {
        *(bool*)userdata1 = true;
        auto* out = (std::pair<bool, string>*)userdata2;
        if ( type != WGPUErrorType_NoError )
        {
            out->first = true;
            if ( msg.data )
                out->second.assign( msg.data, msg.length == WGPU_STRLEN
                                                  ? std::strlen( msg.data ) : msg.length );
        }
    };
    std::pair<bool, string> result{ false, {} };
    callbackInfo.userdata1 = &done;
    callbackInfo.userdata2 = &result;
    Gpu().Device().popErrorScope( callbackInfo );

    // The scope resolves on the device timeline, so it needs pumping before the
    // result is readable.
    for ( i32 i = 0; i < 64 and not done; i++ )
        Gpu().PollDevice();

    failed = result.first;
    if ( failed )
    {
        LogError( "Shader {} failed to compile: {}", name, result.second );
        LogShaderSource( name, source );
        outOk = false;
        return {};
    }
    return module;
}

}


// One WGSL file per shader, holding both entry points.
//
// This replaced the .vert / .frag / .geom triple: WGSL puts every stage in one
// module, so a shader is a single .wgsl file and the default-vertex-stage
// fallback that used to hand a lone fragment shader phong.vert has nothing to
// fall back to. The `#include` expansion is unchanged, and still sets the
// Material and Light module bits that gate the engine's bind groups.
std::optional<ProcessedSource> ParseShader( const path& shaderPath )
{
    path filePath = shaderPath;
    filePath.replace_extension( SHADER_EXTENSION );
    if ( not filesystem::exists( filePath ) )
        return std::nullopt;

    const ShaderModuleTable modules = GetShaderModules();
    return ExpandShaderIncludes( filePath, modules );
}


Ref<Shader> LoadShader( const path& path )
{
    auto processed = ParseShader( path );
    if ( not processed )
    {
        LogError( "No {} file for shader {}", SHADER_EXTENSION, path.string() );
        return nullptr;
    }

    Ref<Shader> shader = CreateRef<Shader>();
    shader->mName = path.stem().string();
    shader->mPath = path;
    shader->mModules = processed->mModules;

    bool ok = false;
    shader->mModule = CompileWGSL( shader->mName, processed->mText, ok );
    if ( not ok )
        return nullptr;

    // Uniform reflection is not implemented yet: WebGPU exposes nothing like
    // glGetActiveUniform, so this has to come from parsing the WGSL ourselves.
    // Until it does, a shader reports no user uniforms, and ShaderComponent
    // rebuilds an empty table rather than a wrong one.
    shader->mUniformDescriptors.clear();
    shader->mUniformDefaults.clear();

    return shader;
}


Ref<Shader> Loader::LoadShader( path shaderPath )
{
    auto [relPath, absPath] = RelAbsFromProjectPath( shaderPath.replace_extension() );

    auto iter = mShaders.find( relPath );
    if ( iter != mShaders.end() )
        return iter->second;

    auto shader = bubble::LoadShader( absPath );
    if ( not shader )
    {
        LogError( "Failed to parse shaders: {}", absPath.string() );
        return nullptr;
    }

    mShaders.emplace( relPath, shader );
    mResourcesGeneration = NextResourcesGeneration();
    return shader;
}

}
