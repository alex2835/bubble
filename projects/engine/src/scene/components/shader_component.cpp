#include "engine/pch/pch.hpp"
#include "engine/scene/components/shader_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "engine/types/array.hpp"
#include "engine/types/string.hpp"
#include "engine/utils/geometry.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
namespace
{
// A value only reaches the shader if its Lua type matches the declared GLSL
// type - ApplyShaderUniforms checks before setting - so a value whose type no
// longer matches is as dead as one whose name is gone.
bool ValueMatchesType( const Object& value, GLSLDataType type )
{
    switch ( type )
    {
        case GLSLDataType::Texture2D: return value.is<Ref<Texture2D>>();
        case GLSLDataType::Float:     return value.is<float>();
        case GLSLDataType::Float2:    return value.is<vec2>();
        case GLSLDataType::Float3:    return value.is<vec3>();
        case GLSLDataType::Float4:    return value.is<vec4>();
        case GLSLDataType::Mat3:      return value.is<mat3>();
        case GLSLDataType::Mat4:      return value.is<mat4>();
        case GLSLDataType::Int:       return value.is<int>();
        case GLSLDataType::Bool:      return value.is<bool>();
        case GLSLDataType::Int2:      return value.is<ivec2>();
        case GLSLDataType::Int3:      return value.is<ivec3>();
        case GLSLDataType::Int4:      return value.is<ivec4>();
    }
    return false;
}

// The value the uniform was given in GLSL, captured when the program linked.
// `uniform vec4 uColor = vec4(1.0)` used to arrive in the inspector as vec4(0):
// the shader's own initializer was thrown away and every float uniform started
// at zero, which for a colour means black.
sol::object DefaultUniformValue( lua_State* lua,
                                 const Shader& shader,
                                 const string& name,
                                 GLSLDataType type )
{
    static const UniformDefault cZero;
    const auto iter = shader.mUniformDefaults.find( name );
    const UniformDefault& value = iter != shader.mUniformDefaults.end() ? iter->second : cZero;
    const f32* f = value.mFloats.data();
    const i32* i = value.mInts.data();

    // A matrix uniform with no initializer reads back as all zeros, which
    // collapses whatever it multiplies and is never what anyone meant. Identity
    // is the only sane starting point, and no shader initializes one to zero.
    const bool allZero = ranges::all_of( value.mFloats, []( f32 v ){ return v == 0.0f; } );

    switch ( type )
    {
        // A sampler's default is the texture unit it reads, which says nothing
        // about which texture belongs there. Left empty for the user to fill.
        case GLSLDataType::Texture2D: return sol::make_object( lua, Ref<Texture2D>() );
        case GLSLDataType::Float:  return sol::make_object( lua, f[0] );
        case GLSLDataType::Float2: return sol::make_object( lua, glm::make_vec2( f ) );
        case GLSLDataType::Float3: return sol::make_object( lua, glm::make_vec3( f ) );
        case GLSLDataType::Float4: return sol::make_object( lua, glm::make_vec4( f ) );
        case GLSLDataType::Mat3:   return sol::make_object( lua, allZero ? mat3( 1 ) : glm::make_mat3( f ) );
        case GLSLDataType::Mat4:   return sol::make_object( lua, allZero ? mat4( 1 ) : glm::make_mat4( f ) );
        case GLSLDataType::Int:    return sol::make_object( lua, i[0] );
        case GLSLDataType::Bool:   return sol::make_object( lua, i[0] != 0 );
        case GLSLDataType::Int2:   return sol::make_object( lua, glm::make_vec2( i ) );
        case GLSLDataType::Int3:   return sol::make_object( lua, glm::make_vec3( i ) );
        case GLSLDataType::Int4:   return sol::make_object( lua, glm::make_vec4( i ) );
    }
    return sol::make_object( lua, sol::lua_nil );
}

} // namespace


static string JoinNames( const vector<string>& names )
{
    string out;
    for ( const auto& name : names )
    {
        if ( not out.empty() )
            out += ", ";
        out += name;
    }
    return out;
}

void LogDroppedShaderUniforms( const Ref<Shader>& shader, const DroppedUniforms& dropped )
{
    if ( dropped.Empty() or not shader )
        return;

    if ( not dropped.mMissing.empty() )
        LogWarning( "Shader '{}': dropped {} saved uniform(s) the shader no longer has - "
                    "removed, renamed, or optimised out because nothing reads them: {}",
                    shader->mName, dropped.mMissing.size(), JoinNames( dropped.mMissing ) );

    if ( not dropped.mRetyped.empty() )
        LogWarning( "Shader '{}': reset {} uniform(s) whose type changed in the shader, "
                    "so the saved value no longer fits: {}",
                    shader->mName, dropped.mRetyped.size(), JoinNames( dropped.mRetyped ) );
}


void ShaderComponent::OnComponentDraw( const Project& project, const Entity& entity, ShaderComponent& shaderComponent )
{
    ImGui::TextColored( TEXT_COLOR, "ShaderComponent" );

    const auto& shader = shaderComponent.mShader;
    auto shaderName = shader ? shader->mName.c_str() : "Not selected";
    if ( ImGui::BeginCombo( "shaders", shaderName ) )
    {
        for ( const auto& [shaderPath, shaderRef] : project.mLoader.mShaders )
        {
            auto shaderComboName = shaderPath.stem().string();
            if ( ImGui::Selectable( shaderComboName.c_str(), shaderComboName == shaderName ) )
            {
                shaderComponent.mShader = shaderRef;

                // The table is keyed by the *old* shader's uniforms. Without
                // this the inspector kept showing them, and the new shader's
                // own uniforms never appeared until the project was reopened.
                auto& scriptingEngine = const_cast<Project&>( project ).mScriptingEngine;
                const auto dropped = shaderComponent.RebuildUniforms( scriptingEngine );
                LogDroppedShaderUniforms( shaderComponent.mShader, dropped );
            }
        }
        ImGui::EndCombo();
    }

    if ( shaderComponent.mUniforms and shaderComponent.mUniforms->is<Table>() )
        DrawAnyValue( const_cast<Project&>( project ), "Uniforms##shader", *shaderComponent.mUniforms, true );
}

void ShaderComponent::ToJson( json& j, const Project& project, const ShaderComponent& shaderComponent )
{
    if ( not shaderComponent.mShader )
    {
        j = nullptr;
        return;
    }

    auto [relPath, _] = project.mLoader.RelAbsFromProjectPath( shaderComponent.mShader->mPath );
    j["Path"] = relPath;

    if ( shaderComponent.mUniforms )
        j["Uniforms"] = SaveAnyValue( *shaderComponent.mUniforms );
}

void ShaderComponent::FromJson( const json& j, Project& project, ShaderComponent& shaderComponent )
{
    if ( j.is_null() )
        return;

    shaderComponent.mShader = project.mLoader.LoadShader( j["Path"] );

    // Load the saved values into a table of their own, then reconcile against
    // it. The shader may have changed since the project was written, and only
    // the uniforms it actually has now should survive - the old code wrote
    // every saved key straight into the rebuilt table, which put uniforms the
    // shader no longer has back into the inspector.
    Table saved = project.mScriptingEngine.CreateTable();
    if ( j.contains( "Uniforms" ) )
    {
        for ( const auto& [key, val] : j["Uniforms"].items() )
            saved[key] = LoadAnyValue( project.mScriptingEngine, val );
    }

    const auto dropped = shaderComponent.RebuildUniforms( project.mScriptingEngine, &saved );
    LogDroppedShaderUniforms( shaderComponent.mShader, dropped );
}

void ShaderComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<ShaderComponent>(
        "ShaderComponent",
        "shader", sol::property(
            []( ShaderComponent& sc ) { return sc.mShader; },
            [&lua]( ShaderComponent& sc, const Ref<Shader>& shader )
            {
                sc.mShader = shader;
                sc.RebuildUniforms( lua );
            }
        ),
        "uniforms", sol::property(
            // Built on demand rather than asserted on. A shader attached from
            // Lua never went through FromJson, so mUniforms was null here - and
            // the assert is compiled out of a release build, which turned
            // reading `.uniforms` into a null dereference.
            [&lua]( ShaderComponent& sc ) -> sol::object
            {
                sc.EnsureUniforms( lua );
                if ( not sc.mUniforms )
                    return sol::make_object( lua, sol::lua_nil );
                return sol::object( sc.mUniforms->as<Table>() );
            },
            [&lua]( ShaderComponent& sc, const Table& t )
            {
                sc.EnsureUniforms( lua );
                if ( sc.mUniforms )
                    *sc.mUniforms = t;
            }
        ),
        sol::meta_function::to_string,
        []( const ShaderComponent& sc ) { return sc.mShader ? sc.mShader->mName : "null"; }
    );
}

ShaderComponent::ShaderComponent( const Ref<Shader>& shader )
    : mShader( shader )
{
}

ShaderComponent::ShaderComponent( const ShaderComponent& shaderComponent )
{
    *this = shaderComponent;
}

ShaderComponent& ShaderComponent::operator=( const ShaderComponent& shaderComponent )
{
    if ( this != &shaderComponent )
    {
        mShader = shaderComponent.mShader;
        mUniforms = AnyDeepCopy( shaderComponent.mUniforms );
    }
    return *this;
}

ShaderComponent::~ShaderComponent()
{
}

DroppedUniforms ShaderComponent::RebuildUniforms( ScriptingEngine& lua )
{
    return RebuildUniforms( *lua.mLua );
}

DroppedUniforms ShaderComponent::RebuildUniforms( ScriptingEngine& lua, const Table* previous )
{
    return RebuildUniforms( *lua.mLua, previous );
}

DroppedUniforms ShaderComponent::RebuildUniforms( sol::state& lua )
{
    if ( mUniforms and mUniforms->is<Table>() )
    {
        const Table previous = mUniforms->as<Table>();
        return RebuildUniforms( lua, &previous );
    }
    return RebuildUniforms( lua, nullptr );
}

void ShaderComponent::EnsureUniforms( sol::state& lua )
{
    if ( mUniforms and mUniforms->is<Table>() )
        return;
    RebuildUniforms( lua, nullptr );
}

DroppedUniforms ShaderComponent::RebuildUniforms( sol::state& lua, const Table* previous )
{
    if ( !mShader )
    {
        mUniforms.reset();
        return {};
    }

    DroppedUniforms dropped;
    auto table = lua.create_table();

    for ( const auto& [name, type] : mShader->mUniformDescriptors )
    {
        const Object carried = previous ? Object( ( *previous )[name] ) : Object();
        const bool hasCarried = carried.valid() and not carried.is<sol::nil_t>();

        if ( hasCarried and ValueMatchesType( carried, type ) )
        {
            table[name] = carried;
            continue;
        }

        // A value that is present but no longer the right type is as lost as a
        // deleted one - ApplyShaderUniforms would skip it forever - so report
        // it rather than leaving a stale number sitting in the inspector.
        if ( hasCarried )
            dropped.mRetyped.push_back( name );

        table[name] = DefaultUniformValue( table.lua_state(), *mShader, name, type );
    }

    // Names the shader has no uniform for at all.
    if ( previous )
    {
        for ( const auto& [key, value] : *previous )
        {
            if ( key.is<string>() and not mShader->mUniformDescriptors.contains( key.as<string>() ) )
                dropped.mMissing.push_back( key.as<string>() );
        }
    }

    mUniforms = CreateScope<Any>( std::move( table ) );
    return dropped;
}

}
