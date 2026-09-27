#include "engine/pch/pch.hpp"
#include "editor_user_interface/windows/window_base.hpp"
#include "editor_application/editor_application.hpp"

namespace bubble
{
UserInterfaceWindowBase::UserInterfaceWindowBase( BubbleEditor& editor )
    : UserInterfaceWindowBase( editor, editor.MainDocument() )
{}

UserInterfaceWindowBase::UserInterfaceWindowBase( BubbleEditor& editor, const EditorDocument& document )
    : mWindow( editor.mWindow ),
      mEditorMode( editor.mEditorMode ),
      mSceneViewport( document.mSceneViewport ),
      mEntityIdViewport( document.mEntityIdViewport ),
      mSceneCamera( document.mCamera ),
      mProject( editor.mProject ),
      mLevel( document.mLevel ),
      mSelection( document.mSelection ),
      mHistory( document.mHistory ),
      mClipboard( document.mClipboard ),
      mPicker( document.mPicker ),
      mPendingRectSelect( document.mPendingRectSelect ),
      mViewportHovered( document.mViewportHovered ),
      mViewManipulatorUsing( document.mViewManipulatorUsing ),
      mEditableWhileRunning( document.mEditableWhileRunning ),
      mOperatorQueue( editor.mOperatorQueue ),
      mUIGlobals( editor.mUIGlobals ),
      mEditorSettings( editor.mEditorSettings )
{}

bool UserInterfaceWindowBase::Editable() const
{
    return mEditableWhileRunning or mEditorMode == EditorMode::Editing;
}

OperatorContext UserInterfaceWindowBase::Operators() const
{
    return OperatorContext{ mProject, mLevel, mHistory, mSelection, mClipboard };
}

}

