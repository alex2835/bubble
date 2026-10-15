#pragma once
#include "bubble/editing/document.hpp"
#include "bubble/reflection/field_path.hpp"
#include "bubble/scene/hierarchy.hpp"

// Every way a document's scene changes: a field, the tree, the components.
// Each is a step of the document's history, or part of its open group. Each
// is checked before anything changes, so an error leaves the document as it
// was; errors name the entity by its path, as a scene file's do:
// "/lamp: light.brightness: a number expected, got "bright"". An entity not
// in the tree is not the document's.
//
// The operators call these; nothing else changes a document.
namespace bubble
{
// The field at `path` in the entity's `component`: "brightness",
// "steps[2]", "props.speed". `value` converts as Reflection::TrySetField
// converts it - a Value goes into a C++ field through TryFromValue.
expected<void, string> TrySetField( Document& document,
                                    Entity entity,
                                    Name component,
                                    const FieldPath& path,
                                    MetaAny value );

// A new entity under `parent`, named as CreateChild names it.
expected<Entity, string> TryCreateEntity( Document& document, Entity parent, const Text& name, size_t index = cAtEnd );
// With everything under it. Its components come back on undo, those of the
// types ComponentTypes knows; anything else on it was a cache, rebuilt.
expected<void, string> TryDestroyEntity( Document& document, Entity entity );
// As TrySetParent moves it: the name made unique under the new parent.
expected<void, string> TryMoveEntity( Document& document, Entity entity, Entity parent, size_t index = cAtEnd );
// Returns the name it got: `name` made fit and unique, as Rename makes it.
expected<Text, string> TryRenameEntity( Document& document, Entity entity, const Text& name );

// With the component's defaults.
expected<void, string> TryAddComponent( Document& document, Entity entity, Name component );
expected<void, string> TryRemoveComponent( Document& document, Entity entity, Name component );
}
