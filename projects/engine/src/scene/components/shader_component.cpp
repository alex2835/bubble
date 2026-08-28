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
                shaderComponent.mShader = shaderRef;
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
    shaderComponent.RebuildUniforms( project.mScriptingEngine );

    if ( j.contains( "Uniforms" ) and
         shaderComponent.mUniforms and
         shaderComponent.mUniforms->is<Table>() )
    {
        auto table = shaderComponent.mUniforms->as<Table>();
        for ( const auto& [key, val] : j["Uniforms"].items() )
            table[key] = LoadAnyValue( project.mScriptingEngine, val );
    }
}

void ShaderComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<ShaderComponent>(
        "ShaderComponent",
        "Shader", &ShaderComponent::mShader,
        "Uniforms", sol::property(
            []( ShaderComponent& sc ) -> sol::object
            {
                assert( sc.mUniforms );
                return sc.mUniforms->as<Table>();
            },
            []( ShaderComponent& sc, const Table& t ) 
            { 
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

void ShaderComponent::RebuildUniforms( ScriptingEngine& lua )
{
    if ( !mShader )
    {
        mUniforms.reset();
        return;
    }
    auto table = lua.CreateTable();
    for ( const auto& [name, type] : mShader->mUniformDescriptors )
    {
        switch ( type )
        {
            case GLSLDataType::Texture2D: table[name] = Ref<Texture2D>(); break;
            case GLSLDataType::Float:  table[name] = 0.0f;       break;
            case GLSLDataType::Float2: table[name] = vec2( 0 );  break;
            case GLSLDataType::Float3: table[name] = vec3( 0 );  break;
            case GLSLDataType::Float4: table[name] = vec4( 0 );  break;
            case GLSLDataType::Mat3:   table[name] = mat3( 1 );  break;
            case GLSLDataType::Mat4:   table[name] = mat4( 1 );  break;
            case GLSLDataType::Int:    table[name] = 0;          break;
            case GLSLDataType::Bool:   table[name] = false;      break;
            case GLSLDataType::Int2:   table[name] = ivec2( 0 ); break;
            case GLSLDataType::Int3:   table[name] = ivec3( 0 ); break;
            case GLSLDataType::Int4:   table[name] = ivec4( 0 ); break;
        }
    }
    mUniforms = CreateScope<Any>( std::move( table ) );
}

}
