
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
    mSelection.mPrejectTreeNode = node;
    mSelection.mEntities.clear();
    FillProjectTreeNodeEntities( mSelection.mEntities, node );

    mSelection.mGroupTransform = TransformComponent();
    // Single entity selected
    // Group selected
    if ( not mSelection.mEntities.empty() )
    {
        float count = 0;
        TransformComponent avgTransform;
        for ( auto entity : mSelection.mEntities )
        {
            if ( not mProject.mScene.HasComponent<TransformComponent>( entity ) )
                continue;
            count++;
            avgTransform = avgTransform + mProject.mScene.GetComponent<TransformComponent>( entity );
        }
        mSelection.mGroupTransform = avgTransform * ( 1.0f / count );
    }
}

}

