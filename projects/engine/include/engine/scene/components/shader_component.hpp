#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/types/any.hpp"

namespace bubble
{
// What reconciling a uniform table against its shader threw away. Split by
// reason, because the explanation a user needs is different for each and this
// report is the only record that the value existed.
struct DroppedUniforms
{
    vector<string> mMissing;   // the shader has no uniform by that name any more
    vector<string> mRetyped;   // still there, but declared as a different type

    bool Empty() const { return mMissing.empty() and mRetyped.empty(); }
};


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

    // Rebuild mUniforms so its keys are exactly the shader's active uniforms,
    // carrying over any value from `previous` whose name *and* type still
    // match. A uniform the shader no longer has is dropped; its name is
    // returned so the caller can report it, since dropping it silently loses
    // whatever was tuned there.
    //
    // "Not active" is broader than "deleted": a uniform nothing reads is
    // optimised out of the linked program and lands here too.
    DroppedUniforms RebuildUniforms( ScriptingEngine& lua, const Table* previous );

    // Reconcile against whatever mUniforms currently holds. This is the form to
    // call after a hot reload, where the shader changed but the values did not.
    DroppedUniforms RebuildUniforms( ScriptingEngine& lua );

    // Same two, for callers that hold the sol::state and not the engine around
    // it - the Lua bindings and the editor's inspector.
    DroppedUniforms RebuildUniforms( sol::state& lua, const Table* previous );
    DroppedUniforms RebuildUniforms( sol::state& lua );

    // Build the table if it is missing, leave it alone if it is not. A shader
    // set from Lua or picked in the inspector arrives with no table at all, and
    // everything downstream - the `uniforms` property, the inspector, the draw
    // loop - assumed one was always there.
    void EnsureUniforms( sol::state& lua );

    Ref<Shader> mShader;
    Scope<Any> mUniforms;
};

// One line per shader rather than one per component: a shader shared by a
// hundred entities would otherwise print the same warning a hundred times.
void LogDroppedShaderUniforms( const Ref<Shader>& shader, const DroppedUniforms& dropped );

}
