#include "engine/pch/pch.hpp"
#include "engine/reflection/reflection.hpp"
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
void ModelComponent::Reflect()
{
    TypeBuilder<ModelComponent>( Name().data() )
        .Field<&ModelComponent::mModel>( "model" );
}

void ModelComponent::BindLuaMethods( sol::state&, sol::usertype<ModelComponent>& type )
{
    type[sol::meta_function::to_string] = []( const ModelComponent& c ) { return c.mModel ? c.mModel->mName : "null"; };
}

ModelComponent::ModelComponent( const Ref<Model>& model )
    : mModel( model )
{
}

ModelComponent::~ModelComponent()
{

}

}
