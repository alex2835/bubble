#pragma once
#include "engine/types/set.hpp"
#include "engine/types/json.hpp"
#include "engine/scene/scene.hpp"
#include <functional>
#include "editor_user_interface/windows/window_base.hpp"

namespace bubble
{
// The scene's tree - its entities, from the root down, as the hierarchy
// holds them - and under it the inspector of the selected entity.
//
// Drag an entity onto another to put it under it; right click for what can
// be made there; F2 renames. Everything that changes the scene is an
// operator or a command, and is asked for as the tree is drawn but done
// after it, since the tree being walked is what it changes.
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
    const Ref<Texture2D>& IconOf( Entity entity ) const;
    string NameOf( Entity entity ) const;

    void DrawEntity( Entity entity );
    void DrawContextMenu( Entity entity );
    void DrawPrefabMenu( Entity entity );
    void DragDrop( Entity entity );
    void Rename( Entity entity, const string& name );
    void Invoke( const char* op, const json& args );
    void RunDeferred();
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

    // F2: the entity being renamed and the text so far.
    Entity mRenaming = Entity::Null;
    string mRenameText;
    bool mFocusRename = false;

    string mPrefabName;
    vector<std::function<void()>> mDeferred;
};

}
