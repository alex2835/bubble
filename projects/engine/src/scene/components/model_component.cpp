#include "engine/pch/pch.hpp"
#include "engine/scene/components/model_component.hpp"
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
void ModelComponent::OnComponentDraw( InspectorContext& ctx, const Entity& entity, ModelComponent& modelComponent )
{
    ImGui::TextColored( TEXT_COLOR, "ModelComponent" );

    const auto& model = modelComponent.mModel;
    ComboProperty<ModelComponent>( ctx, entity, "models", model, model ? model->mName.c_str() : "Not selected",
                                   ctx.mProject.mLoader.mModels,
                                   []( const auto& entry ) { return entry.first.stem().string(); },
                                   []( const auto& entry ) { return entry.second; },
                                   []( ModelComponent& c, const Ref<Model>& v ) { c = v; } );
}

void ModelComponent::ToJson( json& json, const Project& project, const ModelComponent& modelComponent )
{
    if ( not modelComponent.mModel )
    {
        json = nullptr;
        return;
    }
    auto [relPath, _] = project.mLoader.RelAbsFromProjectPath( modelComponent.mModel->mPath );
    json["Path"] = relPath;
}

void ModelComponent::FromJson( const json& json, Project& project, ModelComponent& modelComponent )
{
    if ( not json.is_null() )
        modelComponent = project.mLoader.LoadModel( json["Path"] );
}

void ModelComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<ModelComponent>(
        "ModelComponent",
        "model", &ModelComponent::mModel,
        sol::meta_function::to_string,
        []( const ModelComponent& mc ) { return mc.mModel ? mc.mModel->mName : "null"; }
    );
}

ModelComponent::ModelComponent( const Ref<Model>& model )
    : mModel( model )
{
}

ModelComponent::~ModelComponent()
{

}

}
