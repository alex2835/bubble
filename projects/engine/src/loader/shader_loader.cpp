#include "engine/pch/pch.hpp"
#include "engine/loader/loader.hpp"
#include "engine/types/string.hpp"
#include "engine/loader/shader_module_loader.hpp"
#include "engine/renderer/gpu_context.hpp"
#include "engine/renderer/pipeline.hpp"
#include "engine/log/log.hpp"
#include <wgsl_reflect/wgsl_reflect.hpp>
#include <charconv>

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


// The struct a shader declares its own uniforms in.
//
// Reflection has to find them somewhere, and a fixed name is far less fragile
// than guessing which of a module's blocks belongs to the user: after include
// expansion the engine's own camera, light, material and per-draw blocks are all
// in the same source.
constexpr string_view USER_UNIFORM_STRUCT = "UserUniforms"sv;

std::optional<GLSLDataType> ToGLSLDataType( const wgsl_reflect::Type& type )
{
    using namespace wgsl_reflect;
    switch ( type.mKind )
    {
        case TypeKind::Texture:
            return GLSLDataType::Texture2D;

        case TypeKind::Scalar:
            switch ( type.mScalar )
            {
                case ScalarType::F32: return GLSLDataType::Float;
                case ScalarType::I32: return GLSLDataType::Int;
                case ScalarType::U32: return GLSLDataType::UInt;
                // A WGSL uniform cannot hold a bool - it is not host shareable -
                // so a shader spells one u32. The inspector still edits it as a
                // checkbox, which is what GLSLDataType::Bool means here.
                case ScalarType::Bool: return GLSLDataType::Bool;
                default: return std::nullopt;
            }

        case TypeKind::Vector:
            if ( type.mScalar == ScalarType::F32 )
            {
                if ( type.mRows == 2 ) return GLSLDataType::Float2;
                if ( type.mRows == 3 ) return GLSLDataType::Float3;
                if ( type.mRows == 4 ) return GLSLDataType::Float4;
            }
            if ( type.mScalar == ScalarType::I32 )
            {
                if ( type.mRows == 2 ) return GLSLDataType::Int2;
                if ( type.mRows == 3 ) return GLSLDataType::Int3;
                if ( type.mRows == 4 ) return GLSLDataType::Int4;
            }
            return std::nullopt;

        case TypeKind::Matrix:
            if ( type.mScalar != ScalarType::F32 or type.mColumns != type.mRows )
                return std::nullopt;
            if ( type.mColumns == 3 ) return GLSLDataType::Mat3;
            if ( type.mColumns == 4 ) return GLSLDataType::Mat4;
            return std::nullopt;

        default:
            return std::nullopt;
    }
}

// Reads a @default(...) annotation off the member's trailing comment.
//
// WGSL forbids an initializer on a uniform, so the value a uniform starts at
// has nowhere to live in the declaration itself. Under OpenGL this came from
// glGetUniformfv on the linked program, which recovered whatever the GLSL
// initializer said; a comment is the only place left to put it.
UniformDefault ParseDefault( string_view comment, GLSLDataType type )
{
    UniformDefault result;

    const bool wantsInt = type == GLSLDataType::Int  or type == GLSLDataType::Int2 or
                          type == GLSLDataType::Int3 or type == GLSLDataType::Int4 or
                          type == GLSLDataType::UInt or type == GLSLDataType::Bool;

    const auto open = comment.find( "@default(" );
    if ( open == string_view::npos )
    {
        // No annotation. Identity for a matrix, zero for everything else, which
        // is what DefaultUniformValue already assumed for an unset uniform.
        if ( type == GLSLDataType::Mat3 or type == GLSLDataType::Mat4 )
        {
            const u32 side = type == GLSLDataType::Mat3 ? 3u : 4u;
            for ( u32 i = 0; i < side; i++ )
                result.mFloats[i * side + i] = 1.0f;
        }
        return result;
    }

    const auto close = comment.find( ')', open );
    if ( close == string_view::npos )
        return result;

    const string_view args = comment.substr( open + 9, close - open - 9 );

    u64 floatIndex = 0;
    u64 intIndex = 0;
    u64 start = 0;
    while ( start <= args.size() )
    {
        const auto comma = args.find( ',', start );
        string_view token = args.substr( start, comma == string_view::npos
                                                ? string_view::npos : comma - start );
        while ( not token.empty() and std::isspace( (unsigned char)token.front() ) )
            token.remove_prefix( 1 );
        while ( not token.empty() and std::isspace( (unsigned char)token.back() ) )
            token.remove_suffix( 1 );

        if ( not token.empty() )
        {
            if ( wantsInt )
            {
                if ( intIndex < result.mInts.size() )
                {
                    i32 value = 0;
                    if ( token == "true" )
                        value = 1;
                    else if ( token == "false" )
                        value = 0;
                    else
                        std::from_chars( token.data(), token.data() + token.size(), value );
                    result.mInts[intIndex++] = value;
                }
            }
            else if ( floatIndex < result.mFloats.size() )
            {
                f32 value = 0.0f;
                std::from_chars( token.data(), token.data() + token.size(), value );
                result.mFloats[floatIndex++] = value;
            }
        }

        if ( comma == string_view::npos )
            break;
        start = comma + 1;
    }
    return result;
}

// Fills in the uniforms a shader declares for itself, from the expanded source.
//
// This is what glGetActiveUniform used to provide for free at link time. WebGPU
// exposes no reflection anywhere - not through webgpu.h, not in wgpu-native, not
// in the browser - so the source is the only place the information exists, and
// hot reload needs it for arbitrary user shaders at runtime.
void ReflectUserUniforms( Shader& shader, string_view expandedSource )
{
    shader.mUniformDescriptors.clear();
    shader.mUniformDefaults.clear();
    shader.mUniformOffsets.clear();
    shader.mUserUniformSize = 0;

    const wgsl_reflect::Module module = wgsl_reflect::Parse( expandedSource );
    for ( const auto& error : module.mErrors )
        LogWarning( "Shader {}: {}", shader.mName, error );

    const wgsl_reflect::Struct* uniforms =
        module.FindStruct( string( USER_UNIFORM_STRUCT ) );
    if ( not uniforms )
        return;

    // Every shader's block shares one bind group layout and one slot size, so a
    // block that does not fit would silently run into the next entity's values.
    if ( uniforms->mSize > cUserUniformBlockSize )
    {
        LogError( "Shader {}: {} is {} bytes, which exceeds the {} byte limit",
                  shader.mName, USER_UNIFORM_STRUCT, uniforms->mSize, cUserUniformBlockSize );
        return;
    }
    shader.mUserUniformSize = (u32)uniforms->mSize;

    for ( const auto& member : uniforms->mMembers )
    {
        // Padding a shader author added by hand is not theirs to edit.
        if ( member.mName.starts_with( "_" ) )
            continue;

        const auto type = ToGLSLDataType( member.mType );
        if ( not type )
        {
            LogWarning( "Shader {}: uniform {} has an unsupported type {}",
                        shader.mName, member.mName, member.mType.mSpelling );
            continue;
        }

        shader.mUniformDefaults.emplace( member.mName,
                                         ParseDefault( member.mTrailingComment, *type ) );
        shader.mUniformDescriptors.emplace( member.mName, *type );
        shader.mUniformOffsets.emplace( member.mName, member.mOffset );
    }
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

    ReflectUserUniforms( *shader, processed->mText );

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
