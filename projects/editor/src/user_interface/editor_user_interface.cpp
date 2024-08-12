
#include "editor_user_interface/editor_user_interface.hpp"

namespace bubble
{
EditorUserInterface::EditorUserInterface( EditorState& editorState )
    :  mMenubar( editorState ),
       mEntitiesWindow( editorState ),
       mSceneViewportWindow( editorState ),
       mProjectWindow( editorState )
{

}

void EditorUserInterface::OnUpdate( DeltaTime dt )
{
    mMenubar.OnUpdate( dt );
    mEntitiesWindow.OnUpdate( dt );
    mSceneViewportWindow.OnUpdate( dt );
    mProjectWindow.OnUpdate( dt );
}

void EditorUserInterface::OnDraw( DeltaTime dt )
{
    mMenubar.OnDraw( dt );
    mEntitiesWindow.OnDraw( dt );
    mSceneViewportWindow.OnDraw( dt );
    mProjectWindow.OnDraw( dt );
}

}
