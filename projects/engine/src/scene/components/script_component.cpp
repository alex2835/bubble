#include "engine/pch/pch.hpp"
#include "engine/scene/components/script_component.hpp"
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
void ScriptComponent::OnComponentDraw( InspectorContext& ctx, const Entity& entity, ScriptComponent& scriptComponent )
{
    ImGui::TextColored( TEXT_COLOR, "ScriptComponent" );

    const auto& script = scriptComponent.mScript;
    ComboProperty<ScriptComponent>( ctx, entity, "scripts", script, script ? script->mName.c_str() : "Not selected",
                                    ctx.mProject.mLoader.mScripts,
                                    []( const auto& entry ) { return entry.first.stem().string(); },
                                    []( const auto& entry ) { return entry.second; },
                                    []( ScriptComponent& c, const Ref<Script>& v ) { c = v; } );
}

void ScriptComponent::ToJson( json& json, const Project& project, const ScriptComponent& scriptComponent )
{
    if ( not scriptComponent.mScript )
    {
        json = nullptr;
        return;
    }

    auto [relPath, _] = project.mLoader.RelAbsFromProjectPath( scriptComponent.mScript->mPath );
    json = relPath;
}

void ScriptComponent::FromJson( const json& json, Project& project, ScriptComponent& scriptComponent )
{
    if ( not json.is_null() )
        scriptComponent.mScript = project.mLoader.LoadScript( json );
}

void ScriptComponent::CreateLuaBinding( sol::state& lua )
{

}

ScriptComponent::ScriptComponent( const Ref<Script>& scirpt )
    : mScript( scirpt )
{

}

ScriptComponent::~ScriptComponent()
{

}

}
