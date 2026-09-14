#pragma once

namespace bubble
{
class Project;
class History;

// What an editing UI is handed: the document and where its edits go. Passed
// down into every OnComponentDraw so a widget in a component can push the
// command for what it changed.
struct EditContext
{
    Project& mProject;
    History& mHistory;
};

}
