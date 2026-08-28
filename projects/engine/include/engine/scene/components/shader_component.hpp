#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/types/any.hpp"

namespace bubble
{
struct ShaderComponent
{
    static int ID() { return static_cast<int>( ComponentID::Shader ); }
	static string_view Name() { return "Shader"sv; }

    static void OnComponentDraw( const Project& project, const Entity& entity, ShaderComponent& component );
	static void ToJson( json& json, const Project& project, const ShaderComponent& component );
	static void FromJson( const json& json, Project& project, ShaderComponent& component );
	static void CreateLuaBinding( sol::state& lua );

public:
	ShaderComponent() = default;
	ShaderComponent( const Ref<Shader>& shader );
    ShaderComponent( const ShaderComponent& shaderComponent );
    ShaderComponent& operator= ( const ShaderComponent& shaderComponent );
    ~ShaderComponent();
    void RebuildUniforms( ScriptingEngine& lua );
    Ref<Shader> mShader;
    Scope<Any> mUniforms;
};

}
