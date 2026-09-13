#pragma once
#include "engine/renderer/entity_id_picker.hpp"
#include "engine/utils/filesystem.hpp"
#include "engine/types/utility.hpp"
#include "engine/types/string.hpp"


namespace bubble
{
// UI global state (Common variables for all interface windows and editor)
struct UIGlobals
{
    bool mNeedUpdateProjectFilesWindow = false;
    // A level (relative to the project root) a window asked to open. The
    // editor services it from its own update, where it can also drop the
    // selection, history and clipboard that point into the level being left.
    opt<path> mRequestOpenLevel;
    // Same for a project file, and for a level to create, for the same reason.
    opt<path> mRequestOpenProject;
    opt<string> mRequestNewLevel;
    bool mIsViewportHovered = false;
    bool mIsViewManipulatorUsing = false;

    // Menu
    bool mDrawBoundingBoxes = false;
    bool mDrawPhysicsShapes = false;

    // Entity picking. Lives here because the viewport window asks for a read
    // while the editor's frame loop is what renders the id pass and starts the
    // copy - the pass is only drawn on frames where something asked for it,
    // instead of every frame as it used to be.
    EntityIdPicker mEntityIdPicker;
    bool mPendingRectSelect = false;
};
}