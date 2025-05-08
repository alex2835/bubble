
#include "editor_user_interface/windows/window_base.hpp"
#include "editor_application/editor_application.hpp"

namespace bubble
{
UserInterfaceWindowBase::UserInterfaceWindowBase( BubbleEditor& editor )
    : mWindow( editor.mWindow ),
      mSceneViewport( editor.mSceneViewport ),
      mObjectIdViewport( editor.mObjectIdViewport ),
      mSceneCamera( editor.mSceneCamera ),
      mSelectedEntity( editor.mSelectedEntity ),
      mProject( editor.mProject ),
      mUIGlobals( editor.mUIGlobals )
{}

}

