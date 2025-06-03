#pragma once
#include "engine/types/set.hpp"
#include "engine/project/project_tree.hpp"
#include "editor_user_interface/windows/window_base.hpp"

namespace bubble
{
class EntitiesWindow : public UserInterfaceWindowBase
{
public:
    EntitiesWindow( BubbleEditor& editor );
    ~EntitiesWindow();

    string_view Name();
    void OnUpdate( DeltaTime );
    void OnDraw( DeltaTime );

private:
    const Ref<Texture2D>& GetProjectTreeNodeIcon( const Ref<ProjectTreeNode>& node );
    void DrawSceneTreeNode( Ref<ProjectTreeNode>& node );
    void DrawCreateEntityPopup( Ref<ProjectTreeNode>& node );

    void DrawSelectedEntityComponents();
    void DrawEntities();

private:
    Ref<Texture2D> mLevelIcon;
    Ref<Texture2D> mFolerIcon;

    Ref<Texture2D> mObjectIcon;
    Ref<Texture2D> mLightIcon;
    Ref<Texture2D> mCameraIcon;
    Ref<Texture2D> mPlayerIcon;
    Ref<Texture2D> mScriptIcon;
};

}
