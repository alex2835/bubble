#pragma once
#include "engine/types/any.hpp"

namespace bubble
{
class Project;

// ImGui editor for a value: scalars inline, tables as trees with add/remove.
// `frozen` shows without editing.
void DrawFieldsAdding( Project& project, Table& table, string_view scopeName, bool frozen = false );
Any DrawAnyValue( Project& project, string_view name, Any any, bool frozen = false );

}
