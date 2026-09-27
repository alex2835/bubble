#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/types/array.hpp"
#include <recs/entity.hpp>

namespace bubble
{
// Where an entity hangs in the scene's hierarchy: its parent, and its
// children in order. An entity with neither has no component at all - it is
// a root with nothing under it, and most entities are that.
//
// The parent is the truth and is what a file stores; the children are kept
// alongside so the world transforms can be computed from the roots down and
// a subtree walked without a search. Both are written by the functions in
// engine/scene/hierarchy.hpp and nowhere else, which is what keeps them in
// step.
//
// With a parent, the entity's TransformComponent is relative to the parent's;
// its place in the world is the parent's world transform times its own.
struct HierarchyComponent
{
    static int ID() { return static_cast<int>( ComponentID::Hierarchy ); }
    static string_view Name() { return "Hierarchy"sv; }

    static void OnComponentDraw( InspectorContext& ctx, const Entity& entity, HierarchyComponent& component );
    static void ToJson( json& json, const Project& project, const HierarchyComponent& component );
    static void FromJson( const json& json, Project& project, HierarchyComponent& component );
    static void CreateLuaBinding( sol::state& lua );

    Entity mParent = INVALID_ENTITY;
    vector<Entity> mChildren;
};

}
