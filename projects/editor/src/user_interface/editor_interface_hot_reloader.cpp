
#include "editor_application/editor_application.hpp"
#include "editor_interfaces/editor_interface_hot_reloader.hpp"

namespace bubble
{
EditorInterfaceHotReloader::EditorInterfaceHotReloader( EditorState& editorState )
    :  mMenubar( editorState ),
       mEntitiesInterface( editorState ),
       mSceneViewportInterface( editorState ),
       mProjectInterface( editorState )
{

}

void EditorInterfaceHotReloader::OnUpdate( DeltaTime dt )
{
    mMenubar.OnUpdate( dt );
    mEntitiesInterface.OnUpdate( dt );
    mSceneViewportInterface.OnUpdate( dt );
    mProjectInterface.OnUpdate( dt );
}

void EditorInterfaceHotReloader::OnDraw( DeltaTime dt )
{
    mMenubar.OnDraw( dt );
    mEntitiesInterface.OnDraw( dt );
    mSceneViewportInterface.OnDraw( dt );
    mProjectInterface.OnDraw( dt );
}

}
