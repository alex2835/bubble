#include "engine/pch/pch.hpp"
#include "engine/reflection/reflection.hpp"
#include "engine/scene/components/audio_listener_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
void AudioListenerComponent::Reflect()
{
    TypeBuilder<AudioListenerComponent>( Name().data() )
        .Note( "Position and orientation come from this entity's transform." )
        .Field<&AudioListenerComponent::mActive>( "active" );
}
}
