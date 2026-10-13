#pragma once
#include "bubble/scene/component_types.hpp"
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
}
