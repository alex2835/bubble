#pragma once
#include "bubble/data/json.hpp"
#include "bubble/scene/component_types.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/scene.hpp"

namespace bubble
{
// A scene as a file: JSON with the tree nested as it hangs.
//
//     {
//       "version": 1,
//       "root": {
//         "children": [
//           {
//             "name": "lamp",
//             "components": {
//               "light": { "color": [1, 0.5, 0], "brightness": 3 }
//             }
//           }
//         ]
//       }
//     }
//
// Children in their order, components in the order ComponentTypes has
// them, fields in the order they are described, an empty "components" or
// "children" left out: a scene saved twice is the same file. Entities
// outside the tree are not saved.
constexpr int cSceneFileVersion = 1;

// Fails on a field reflection cannot keep as a value.
expected<string, string> TrySaveScene( const Scene& scene, const ComponentTypes& componentTypes );

// Strict, as a file is merged and edited by hand: an unknown component or
// field, a name twice among siblings, a version this engine does not read
// is an error that says where it is - "12:9: /lamp: light.brightness: a
// number expected, got "bright"" - not a guess.
expected<Scene, string> TryLoadScene( string_view text, const ComponentTypes& componentTypes );

// One entity as a file holds it - its name, its components, what is under
// it - for a copy kept apart from any file: what the editor copies and
// pastes.
expected<Json, string> TryEntityToJson( const Scene& scene, Entity entity, const ComponentTypes& componentTypes );
// Makes the entity `json` holds under `parent`, at `index`, its name made
// unique there as CreateChild makes it; what is under it is read as
// strictly as a file. On an error nothing is left of it.
expected<Entity, string> TryEntityFromJson( Scene& scene,
                                            const Json& json,
                                            Entity parent,
                                            const ComponentTypes& componentTypes,
                                            size_t index = cAtEnd );
}
