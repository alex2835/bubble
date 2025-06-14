#include "engine/pch/pch.hpp"
#include "editor_user_interface/windows/window_base.hpp"
#include "editor_application/editor_application.hpp"

namespace bubble
{
UserInterfaceWindowBase::UserInterfaceWindowBase( BubbleEditor& editor )
    : mWindow( editor.mWindow ),
      mEditorMode( editor.mEditorMode ),
      mSceneViewport( editor.mSceneViewport ),
      mEntityIdViewport( editor.mEntityIdViewport ),
      mSceneCamera( editor.mSceneCamera ),
      mSelection( editor.mSelection ),
      mProject( editor.mProject ),
      mUIGlobals( editor.mUIGlobals )
{}

}

