#pragma once
#include "engine/types/set.hpp"
#include "engine/project/project_tree.hpp"
#include "engine/types/json.hpp"
#include <functional>
#include "editor_user_interface/windows/window_base.hpp"

namespace bubble
{
class ProjectTreeWindow : public UserInterfaceWindowBase
{
public:
    ProjectTreeWindow( BubbleEditor& editor );
    ProjectTreeWindow( BubbleEditor& editor, const EditorDocument& document );
    ~ProjectTreeWindow();

    string_view Name();
    void OnUpdate( DeltaTime );
    void OnDraw( DeltaTime );
    // The tree and the inspector, into whatever window is current: its own,
    // or the prefab editor's.
    void DrawContent();

private:
    const Ref<Texture2D>& GetProjectTreeNodeIcon( const Ref<ProjectTreeNode>& node );
    void SetSelectionByNode( const Ref<ProjectTreeNode>& node );

    void DrawSceneTreeNode( Ref<ProjectTreeNode>& node, bool isSelected = false );
    void Invoke( const char* op, const json& args );
    void RunDeferred();
    void DrawCreateEntityPopup( Ref<ProjectTreeNode>& node );
    void DragDropNode( const Ref<ProjectTreeNode>& node );
    void DrawPrefabMenu( const Ref<ProjectTreeNode>& node );
    // Where something made or dropped in lands: in front of the camera in a
    // level, at the origin in a prefab.
    vec3 SpawnPoint() const;

    void DrawSelectedEntityComponents();
    void DrawEntities();


private:
    Ref<Texture2D> mLevelIcon;
    Ref<Texture2D> mFolerIcon;
    Ref<Texture2D> mObjectIcon;
    Ref<Texture2D> mPhysicsObjectIcon;
    Ref<Texture2D> mLightIcon;
    Ref<Texture2D> mCameraIcon;
    Ref<Texture2D> mPlayerIcon;
    Ref<Texture2D> mScriptIcon;
    Ref<Texture2D> mAudioIcon;
    string mPrefabName;
    vector<std::function<void()>> mDeferred;
};

}
