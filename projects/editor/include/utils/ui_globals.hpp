#pragma once
#include "engine/renderer/entity_id_picker.hpp"


namespace bubble
{
// UI global state (Common variables for all interface windows and editor)
struct UIGlobals
{
    bool mNeedUpdateProjectFilesWindow = false;
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