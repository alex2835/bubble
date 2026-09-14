#pragma once
#include <imgui.h>
#include "engine/editing/ui/inspector_context.hpp"
#include "engine/editing/ui/interaction.hpp"
#include "engine/editing/history.hpp"
#include "engine/editing/commands/property_command.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"

// ImGui widgets that leave an undo step behind. For the component .cpp files
// only, like component_draw_utils.hpp.
//
// The shape is the same for every one of them: the widget edits a copy, a
// change is applied to the component at once (the view has to follow the
// mouse), and one SetPropertyCommand is recorded per interaction - for a
// drag, when the mouse is released, from the value the drag started at; for a
// checkbox or a combo, on the frame it changed.
namespace bubble
{
// The general form. `value` is the property as it is now; `widget( T& )`
// draws it and returns true when it changed it; `apply( Component&, const T& )`
// writes a value onto the component - the same function the command uses on
// undo and redo. Returns what the widget returned.
template <typename Component, typename T, typename Widget, typename Apply>
bool EditProperty( InspectorContext& ctx, Entity entity, string_view name, T value, Widget&& widget, Apply&& apply )
{
    Scene& scene = ctx.mProject.mLevel.mScene;
    const T before = value;
    const bool changed = widget( value );
    if ( changed )
        apply( scene.GetComponent<Component>( entity ), value );

    edit_detail::TrackEdit( changed, before, value, [&]( const T& from, const T& to )
    {
        ctx.mHistory.Record( CreateScope<SetPropertyCommand<Component, T>>(
            scene, entity, std::format( "{}.{}", Component::Name(), name ), from, to,
            typename SetPropertyCommand<Component, T>::Apply( apply ) ) );
    } );
    return changed;
}

// A plain data member. `Component` is always spelled out at the call: a
// member inherited from a base (TransformComponent : Transform) has a
// pointer-to-member of the base's type, which would deduce the wrong scene
// component.
template <typename Component, typename Base, typename T, typename Widget>
    requires std::derived_from<Component, Base>
bool EditField( InspectorContext& ctx, Entity entity, string_view name, T Base::* member, Widget&& widget )
{
    const T& current = ctx.mProject.mLevel.mScene.GetComponent<Component>( entity ).*member;
    return EditProperty<Component>( ctx, entity, name, current, std::forward<Widget>( widget ),
                                    [member]( Component& c, const T& v ) { c.*member = v; } );
}

/// The usual widgets over a data member. The label is also the step's name.

template <typename Component, typename Base>
bool DragFloatField( InspectorContext& ctx, Entity entity, const char* label, f32 Base::* member,
                     f32 speed = 1.0f, f32 min = 0.0f, f32 max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0 )
{
    return EditField<Component>( ctx, entity, label, member, [&]( f32& v ) { return ImGui::DragFloat( label, &v, speed, min, max, format, flags ); } );
}

template <typename Component, typename Base>
bool DragFloat3Field( InspectorContext& ctx, Entity entity, const char* label, vec3 Base::* member,
                      f32 speed = 1.0f, f32 min = 0.0f, f32 max = 0.0f )
{
    return EditField<Component>( ctx, entity, label, member, [&]( vec3& v ) { return ImGui::DragFloat3( label, &v.x, speed, min, max ); } );
}

template <typename Component, typename Base>
bool SliderFloatField( InspectorContext& ctx, Entity entity, const char* label, f32 Base::* member,
                       f32 min, f32 max, const char* format = "%.3f", ImGuiSliderFlags flags = 0 )
{
    return EditField<Component>( ctx, entity, label, member, [&]( f32& v ) { return ImGui::SliderFloat( label, &v, min, max, format, flags ); } );
}

template <typename Component, typename Base>
bool CheckboxField( InspectorContext& ctx, Entity entity, const char* label, bool Base::* member )
{
    return EditField<Component>( ctx, entity, label, member, [&]( bool& v ) { return ImGui::Checkbox( label, &v ); } );
}

template <typename Component, typename Base>
bool ColorEdit3Field( InspectorContext& ctx, Entity entity, const char* label, vec3 Base::* member )
{
    return EditField<Component>( ctx, entity, label, member, [&]( vec3& v ) { return ImGui::ColorEdit3( label, &v.x ); } );
}

template <typename Component, typename Base>
bool InputTextField( InspectorContext& ctx, Entity entity, const char* label, string Base::* member )
{
    return EditField<Component>( ctx, entity, label, member, [&]( string& v ) { return ImGui::InputText( label, v ); } );
}

// A combo over `items` (any range: a loader map works), each shown by
// `itemName( item )` and standing for the value `itemValue( item )`; picking
// one becomes the step. `current` is what the property holds now.
template <typename Component, typename T, typename Items, typename ItemName, typename ItemValue, typename Apply>
bool ComboProperty( InspectorContext& ctx,
                    Entity entity,
                    const char* label,
                    const T& current,
                    const char* currentName,
                    const Items& items,
                    ItemName&& itemName,
                    ItemValue&& itemValue,
                    Apply&& apply )
{
    return EditProperty<Component>( ctx, entity, label, current,
        [&]( T& v )
        {
            bool changed = false;
            if ( ImGui::BeginCombo( label, currentName ) )
            {
                for ( const auto& item : items )
                {
                    const string name = itemName( item );
                    const T value = itemValue( item );
                    const bool selected = value == v;
                    if ( ImGui::Selectable( name.c_str(), selected ) and not selected )
                    {
                        v = value;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            return changed;
        },
        std::forward<Apply>( apply ) );
}

}
