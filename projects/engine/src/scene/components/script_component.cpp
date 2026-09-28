#include "engine/pch/pch.hpp"
#include "engine/reflection/reflection.hpp"
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
void ScriptComponent::Reflect()
{
    TypeBuilder<ScriptComponent>( Name().data() )
        .Field<&ScriptComponent::mScript>( "script" );
}

ScriptComponent::ScriptComponent( const Ref<Script>& scirpt )
    : mScript( scirpt )
{

}

ScriptComponent::~ScriptComponent()
{

}

}
