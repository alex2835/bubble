#include "engine/pch/pch.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/scene/components/model_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/utils/geometry.hpp"

namespace bubble
{
opt<AABB> TryGetEntityBBox( const Project& project, Entity entity )
{
    const auto* modelComponetPtr = TryGetComponent<ModelComponent>( project, entity );
    const auto* transComponetPtr = TryGetComponent<TransformComponent>( project, entity );
    if ( modelComponetPtr and modelComponetPtr->mModel and transComponetPtr )
        return CalculateTransformedBBox( modelComponetPtr->mModel->mBBox, transComponetPtr->ScaleMat() );
    return std::nullopt;
}

}
