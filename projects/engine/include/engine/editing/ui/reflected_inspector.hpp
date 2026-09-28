#pragma once
#include "engine/scene/components/component_base.hpp"

namespace bubble
{
struct InspectorContext;

// A reflected component drawn from its description: a widget per field,
// chosen by the field's type and FieldInfo; described types and sequences
// opened up as a tree. Each edit applies at once through engine/reflection
// (so OnChanged runs) and becomes one SetFieldCommand per interaction - for a
// drag, when the mouse is released.
void DrawComponentFields( InspectorContext& ctx, Entity entity, ComponentTypeId componentId );

// "outer_cut_off" -> "Outer cut off": a field name as a label.
string FieldLabel( string_view name );

}
