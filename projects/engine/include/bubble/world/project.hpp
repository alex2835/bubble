#pragma once
#include "bubble/assets/asset_catalog.hpp"
#include "bubble/input/action_map.hpp"
#include "bubble/types/string.hpp"

namespace bubble
{
// A project's own settings: project.bubble at its root, JSON.
//
//     {
//       "version": 1,
//       "name": "sandbox",
//       "start_level": { "asset": "<id>", "path": "levels/main.scene" },
//       "actions": {
//         "move_x": [ { "key": "a", "scale": -1 }, { "key": "d" }, { "gamepad_axis": "left_x" } ],
//         "jump": [ { "key": "space" }, { "gamepad_button": "south" } ],
//         "look_x": [ { "mouse_axis": "x", "scale": 0.004 } ],
//         "fire": [ { "mouse_button": "left" }, { "touch": [ 0.5, 0.5, 1, 1 ] } ]
//       }
//     }
//
// An action's bindings are its inputs, each one of "key", "mouse_button",
// "mouse_axis", "gamepad_button", "gamepad_axis" and "touch" - an area of the
// window as shares, left, top, right, bottom -, with a "scale" (1 when not
// said). The names are the controls' own (bubble/input/controls.hpp).
struct ProjectSettings
{
    string mName;
    // As the file links it: an id, a path, or both - read before the
    // project's scan knows its assets.
    AssetId mStartLevelId;
    opt<AssetPath> mStartLevelPathMaybe;
    ActionMap mActions;

    // The start level among the catalog's assets: by its id, else by its
    // path; nothing when the file names none, or one the project lacks.
    opt<AssetId> TryStartLevel( const AssetCatalog& catalog ) const;
};

constexpr string_view cProjectFile = "project.bubble";

// The actions' names in their order: what scripts are checked against.
vector<string> ActionNames( const ActionMap& actions );

// The error says where, as Json does, and what is wrong: "7:24:
// actions.jump[0].key: no key spce" - names listed for what has few.
expected<ProjectSettings, string> TryReadProjectSettings( string_view text );
}
