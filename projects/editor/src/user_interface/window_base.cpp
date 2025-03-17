
#include "editor_user_interface/windows/window_base.hpp"
#include "editor_application/editor_application.hpp"

namespace bubble
{
UserInterfaceWindowBase::UserInterfaceWindowBase( BubbleEditor& editorState )
    : mWindow( editorState.mWindow ),
      mSceneViewport( editorState.mSceneViewport ),
      mObjectIdViewport( editorState.mObjectIdViewport ),
      mSceneCamera( editorState.mSceneCamera ),
      //mEngine( editorState.mEngine ),
      mSelectedEntity( editorState.mSelectedEntity ),
      mProject( editorState.mProject ),
      mUINeedUpdateProjectWindow( editorState.mUINeedUpdateProjectWindow )
{}

}

