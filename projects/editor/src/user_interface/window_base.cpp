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

void UserInterfaceWindowBase::SetSeleciton( const Ref<ProjectTreeNode>& node )
{
    mSelection.mProjectTreeNode = node;
    mSelection.mEntities.clear();
    FillProjectTreeNodeEntities( mSelection.mEntities, node );
    
    // Group selected
    if ( not mSelection.mEntities.empty() )
    {
        float count = 0;
        auto avgPos = vec3( 0 );
        for ( auto entity : mSelection.mEntities )
        {
            if ( not mProject.mScene.HasComponent<TransformComponent>( entity ) )
                continue;
            avgPos += mProject.mScene.GetComponent<TransformComponent>( entity ).mPosition;
            count++;
        }
        avgPos *= ( 1.0f / count );
        mSelection.mGroupTransform = TransformComponent{
            .mPosition=avgPos,
        };
    }
}

}

