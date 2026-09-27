#pragma once

namespace bubble
{
class Project;
class History;
class Scene;

// What an editing UI is handed: the document and where its edits go. Passed
// down into every OnComponentDraw so a widget in a component can push the
// command for what it changed.
//
// The scene is named separately from the project: it is the open level's in
// the main windows, and a prefab's in the prefab editor, while the project -
// resources, the Lua VM - is the same one.
struct InspectorContext
{
    Project& mProject;
    Scene& mScene;
    History& mHistory;
};

}
