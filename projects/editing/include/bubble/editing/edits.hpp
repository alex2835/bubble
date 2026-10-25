#pragma once
#include "bubble/editing/clipboard.hpp"
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
// A move and a rename keep links: every EntityPath in a component - a field,
// or an entity in a Value - that led to the entity or to what is under it
// leads there still, as do the relative links they hold themselves. They are
// set in the same step, an absolute link to an absolute path, a relative one
// to a relative path; a link that led nowhere is left as it is.
//
// As TrySetParent moves it: the name made unique under the new parent.
expected<void, string> TryMoveEntity( Document& document, Entity entity, Entity parent, size_t index = cAtEnd );
// Returns the name it got: `name` made fit and unique, as Rename makes it.
expected<Text, string> TryRenameEntity( Document& document, Entity entity, const Text& name );

// The clipboard's entities under `parent`, at `index` among its children or
// after the last, each named as CreateChild names it - a chair beside a
// chair becomes chair_2. An absolute link inside what was copied leads into
// the copy; every other link is as it was copied. Returns what was made, in
// the clipboard's order.
expected<vector<Entity>, string> TryPasteEntities( Document& document,
                                                   const Clipboard& clipboard,
                                                   Entity parent,
                                                   size_t index = cAtEnd );
// A copy of each of `entities` right after it, as a paste makes one: links
// among them lead among the copies. The clipboard is left alone. Give the top
// of a selection. Returns the copies, in the order of `entities`.
expected<vector<Entity>, string> TryDuplicateEntities( Document& document, span<const Entity> entities );

// With the component's defaults.
expected<void, string> TryAddComponent( Document& document, Entity entity, Name component );
expected<void, string> TryRemoveComponent( Document& document, Entity entity, Name component );
}
