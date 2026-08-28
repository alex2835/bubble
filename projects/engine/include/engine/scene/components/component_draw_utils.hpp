#pragma once
#include <imgui.h>
#include "engine/types/utility.hpp"
#include "engine/scene/components/component_base.hpp"
#include "engine/project/project.hpp"

// Helpers shared by the per-component OnComponentDraw implementations.
// Included by the component .cpp files only - never by a component header,
// which would make engine/project/project.hpp circular.
namespace bubble
{
constexpr auto TEXT_COLOR = ImVec4( 1, 1, 0, 1 );

template <typename T>
const T* TryGetComponent( const Project& project, Entity entity )
{
    if ( not project.mScene.HasComponent<T>( entity ) )
        return nullptr;
    return &project.mScene.GetComponent<T>( entity );
}

opt<AABB> TryGetEntityBBox( const Project& project, Entity entity );

}
