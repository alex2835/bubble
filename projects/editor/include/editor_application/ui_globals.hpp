#pragma once


namespace bubble
{
// UI global state (Common variables for all interface windows and editor)
struct UIGlobals
{
    bool mNeedUpdateProjectWindow = false;
    bool mIsViewportHovered = false;
    bool mIsViewManipulatorUsing = false;

    // Menu
    bool mDrawBoundingBoxes = false;
    bool mDrawPhysicsShapes = false;
};
}