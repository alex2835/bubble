#include "engine/pch/pch.hpp"
#include "engine/utils/snake_case.hpp"
#include "editor_user_interface/windows/project_tree_window.hpp"
#include "editor_application/editor_application.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/scene/hierarchy.hpp"
#include "engine/editing/operators/operator.hpp"
#include "engine/editing/commands/tree_commands.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "engine/utils/imgui_utils.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include <imgui.h>
#include "engine/scene/components/audio_source_component.hpp"
#include "engine/scene/components/camera_component.hpp"
#include "engine/scene/components/character_controller_component.hpp"
#include "engine/scene/components/folder_component.hpp"
#include "engine/scene/components/hierarchy_component.hpp"
#include "engine/scene/components/light_component.hpp"
#include "engine/scene/components/model_component.hpp"
#include "engine/scene/components/prefab_instance_component.hpp"
#include "engine/scene/components/rigid_body_component.hpp"
#include "engine/scene/components/script_component.hpp"
#include "engine/scene/components/tag_component.hpp"


namespace bubble
{
namespace
{
constexpr auto cTreeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen |
                                ImGuiTreeNodeFlags_SpanAllColumns |
                                ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                ImGuiTreeNodeFlags_OpenOnArrow;
}


ProjectTreeWindow::ProjectTreeWindow( BubbleEditor& editor )
    : ProjectTreeWindow( editor, editor.MainDocument() )
{
}

ProjectTreeWindow::ProjectTreeWindow( BubbleEditor& editor, const EditorDocument& document )
    : UserInterfaceWindowBase( editor, document )
{
    mLevelIcon = LoadTexture2D( "./resources/images/icons/scene.png" );
    mFolerIcon = LoadTexture2D( "./resources/images/icons/folder.png" );
    mObjectIcon = LoadTexture2D( "./resources/images/icons/object.png" );
    mPhysicsObjectIcon = LoadTexture2D( "./resources/images/icons/physics.png" );
    mLightIcon = LoadTexture2D( "./resources/images/icons/light.png" );
    mCameraIcon = LoadTexture2D( "./resources/images/icons/camera.png" );
    mPlayerIcon = LoadTexture2D( "./resources/images/icons/player.png" );
    mScriptIcon = LoadTexture2D( "./resources/images/icons/script.png" );
    mAudioIcon = LoadTexture2D( "./resources/images/icons/audio.png" );
}

ProjectTreeWindow::~ProjectTreeWindow()
{
}

string_view ProjectTreeWindow::Name()
{
    return "Entities"sv;
}

void ProjectTreeWindow::OnUpdate( DeltaTime )
{
}

// What an entity is, by what it has - the first that applies.
const Ref<Texture2D>& ProjectTreeWindow::IconOf( Entity entity ) const
{
    const Scene& scene = mLevel.mScene;
    if ( entity == scene.Root() )
        return mLevelIcon;
    if ( scene.HasComponent<FolderComponent>( entity ) )
        return mFolerIcon;
    if ( scene.HasComponent<CameraComponent>( entity ) )
        return mCameraIcon;
    if ( scene.HasComponent<LightComponent>( entity ) )
        return mLightIcon;
    if ( scene.HasComponent<CharacterControllerComponent>( entity ) )
        return mPlayerIcon;
    if ( scene.HasComponent<RigidBodyComponent>( entity ) )
        return mPhysicsObjectIcon;
    if ( scene.HasComponent<AudioSourceComponent>( entity ) )
        return mAudioIcon;
    if ( scene.HasComponent<ScriptComponent>( entity ) and not scene.HasComponent<ModelComponent>( entity ) )
        return mScriptIcon;
    return mObjectIcon;
}

string ProjectTreeWindow::NameOf( Entity entity ) const
{
    const Scene& scene = mLevel.mScene;
    return bubble::NameOf( scene, entity );
}

// Deferred to the end of DrawEntities: most of these change the scene, and
// the tree is being walked - down children vectors a move or a delete would
// shift - while they are asked for.
void ProjectTreeWindow::Invoke( const char* op, const json& args )
{
    mDeferred.push_back( [this, name = string( op ), args]()
    {
        try
        {
            OperatorContext ctx = Operators();
            InvokeOperator( name, ctx, args );
        }
        catch ( const std::exception& e )
        {
            LogError( "{}: {}", name, e.what() );
        }
    } );
}

void ProjectTreeWindow::RunDeferred()
{
    auto deferred = std::move( mDeferred );
    mDeferred.clear();
    for ( auto& action : deferred )
        action();
}

vec3 ProjectTreeWindow::SpawnPoint() const
{
    // TODO: raycast into the scene.
    return mEditableWhileRunning ? vec3( 0 ) : mSceneCamera.mPosition + mSceneCamera.mForward * 30.0f;
}

void ProjectTreeWindow::Rename( Entity entity, const string& name )
{
    Scene& scene = mLevel.mScene;
    if ( not scene.HasComponent<TagComponent>( entity ) or name.empty() )
        return;
    // A name a sibling has gets a number.
    Invoke( "scene.rename", { { "entity", (u64)entity }, { "name", name } } );
}

void ProjectTreeWindow::DrawContextMenu( Entity entity )
{
    if ( not ImGui::BeginPopup( "Entity popup" ) )
        return;

    // What each kind starts with is the operator's business; the menu only
    // names the kinds.
    static constexpr std::pair<const char*, EntityKind> kinds[] = {
        { "Create folder",         EntityKind::Folder },
        { "Create Model Object",   EntityKind::ModelObject },
        { "Create Physics Object", EntityKind::PhysicsObject },
        { "Create Game Object",    EntityKind::GameObject },
        { "Create Script",         EntityKind::Script },
        { "Create Light",          EntityKind::Light },
        { "Create Camera",         EntityKind::Camera },
        { "Create Audio",          EntityKind::Audio },
    };
    const vec3 spawnAt = SpawnPoint();
    for ( const auto& [label, kind] : kinds )
        if ( ImGui::MenuItem( label ) )
            Invoke( "scene.create_node", { { "type", ToSnakeCase( magic_enum::enum_name( kind ) ) },
                                           { "parent", (u64)entity },
                                           { "spawn_at", spawnAt } } );

    if ( entity != mLevel.mScene.Root() )
    {
        ImGui::Separator();
        if ( ImGui::MenuItem( "Rename", "F2" ) )
        {
            mRenaming = entity;
            mRenameText = NameOf( entity );
            mFocusRename = true;
        }
        if ( ImGui::MenuItem( "Delete", "Del" ) )
            Invoke( "scene.delete", { { "entity", (u64)entity } } );
        DrawPrefabMenu( entity );
    }
    ImGui::EndPopup();
}

void ProjectTreeWindow::DrawPrefabMenu( Entity entity )
{
    Scene& scene = mLevel.mScene;
    ImGui::Separator();
    if ( ImGui::BeginMenu( "Save as prefab" ) )
    {
        if ( mPrefabName.empty() )
            mPrefabName = std::format( "prefabs/{}", NameOf( entity ) );
        ImGui::SetNextItemWidth( 220.0f * mWindow.GetUIScale() );
        ImGui::InputText( "##prefab", mPrefabName );
        ImGui::SameLine();
        if ( ImGui::Button( "Save" ) and not mPrefabName.empty() )
        {
            Invoke( "prefab.save", { { "file", mPrefabName }, { "entity", (u64)entity } } );
            mUIGlobals.mNeedUpdateProjectFilesWindow = true;
            mPrefabName.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::TextDisabled( "Relative to the project; .prefab is added." );
        ImGui::EndMenu();
    }

    if ( not scene.HasComponent<PrefabInstanceComponent>( entity ) )
        return;
    const string prefab = scene.GetComponent<PrefabInstanceComponent>( entity ).mPrefab;
    if ( ImGui::MenuItem( std::format( "Open {}", prefab ).c_str() ) )
        mOperatorQueue.Enqueue( "prefab.edit", { { "file", prefab } } );
    if ( ImGui::MenuItem( "Update from prefab" ) )
        Invoke( "prefab.update_instances", { { "file", prefab } } );
}

// Drag an entity onto another to put it under it; the dragged one keeps its
// place in the world. A prefab dragged in from the Project window is an
// instance under the one it is dropped on.
void ProjectTreeWindow::DragDrop( Entity entity )
{
    Scene& scene = mLevel.mScene;
    if ( entity != scene.Root() and ImGui::BeginDragDropSource() )
    {
        const u64 id = (u64)entity;
        ImGui::SetDragDropPayload( "SCENE_ENTITY", &id, sizeof( id ) );
        ImGui::Text( "%s", NameOf( entity ).c_str() );
        ImGui::EndDragDropSource();
    }
    if ( ImGui::BeginDragDropTarget() )
    {
        if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( "PREFAB_FILE" ) )
            Invoke( "prefab.instantiate", { { "file", string( (const char*)payload->Data ) },
                                            { "parent", (u64)entity },
                                            { "spawn_at", SpawnPoint() } } );
        if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( "SCENE_ENTITY" ) )
        {
            // scene.move says why a drop into what is under it is refused.
            // Onto itself - let go where it was picked up - or onto its own
            // parent is no move at all.
            const Entity dragged = Entity::FromId( *(const u64*)payload->Data );
            if ( dragged != entity and ParentOf( scene, dragged ) != entity )
                Invoke( "scene.move", { { "entity", (u64)dragged }, { "parent", (u64)entity } } );
        }
        ImGui::EndDragDropTarget();
    }
}

void ProjectTreeWindow::DrawEntity( Entity entity )
{
    Scene& scene = mLevel.mScene;
    const bool isRoot = entity == scene.Root();
    const bool selected = mSelection.GetEntities().contains( entity );
    const auto children = ChildrenOf( scene, entity );

    ImGui::PushID( (int)(u64)entity );
    ImGui::Image( (ImTextureID)IconOf( entity )->ImTextureId(), ImVec2{ 18, 18 } );
    ImGui::SameLine();

    ImGuiTreeNodeFlags flags = cTreeNodeFlags;
    if ( selected )
        flags |= ImGuiTreeNodeFlags_Selected;
    if ( children.empty() )
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    string label = NameOf( entity );
    if ( not isRoot and scene.HasComponent<PrefabInstanceComponent>( entity ) )
        label += "  [prefab]";
    const bool renaming = mRenaming == entity;
    const bool open = ImGui::TreeNodeEx( "node", flags, "%s", renaming ? "" : label.c_str() );

    if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) or ImGui::IsItemClicked( ImGuiMouseButton_Right ) )
        mSelection.Select( entity, scene );
    if ( ImGui::IsItemHovered() and ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
        ImGui::OpenPopup( "Entity popup" );
    if ( not isRoot and ImGui::IsItemHovered() and ImGui::IsKeyPressed( ImGuiKey_F2 ) )
    {
        mRenaming = entity;
        mRenameText = NameOf( entity );
        mFocusRename = true;
    }
    DragDrop( entity );
    DrawContextMenu( entity );

    if ( renaming )
    {
        ImGui::SameLine();
        if ( mFocusRename )
        {
            ImGui::SetKeyboardFocusHere();
            mFocusRename = false;
        }
        ImGui::SetNextItemWidth( 180.0f * mWindow.GetUIScale() );
        const auto inputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
        if ( ImGui::InputText( "##rename", mRenameText, inputFlags ) )
        {
            Rename( entity, mRenameText );
            mRenaming = Entity::Null;
        }
        else if ( ImGui::IsItemDeactivated() )
            mRenaming = Entity::Null;
    }

    if ( open and not children.empty() )
    {
        // Copied: the list must not be looked into while it may change -
        // changes are deferred, but a span into it would dangle all the same.
        const vector<Entity> list( children.begin(), children.end() );
        for ( const Entity child : list )
            DrawEntity( child );
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void ProjectTreeWindow::DrawEntities()
{
    ImGui::BeginChild( "Entities", ImVec2( 0, 400 ), ImGuiChildFlags_ResizeY );
    if ( Editable() and mLevel.mScene.HasEntity( mLevel.mScene.Root() ) )
        DrawEntity( mLevel.mScene.Root() );
    ImGui::EndChild();
    RunDeferred();
}

void ProjectTreeWindow::DrawSelectedEntityComponents()
{
    BUBBLE_ASSERT( mSelection.GetEntities().size() == 1, "Draw only one entity selected" );
    auto selectedEntity = *mSelection.GetEntities().begin();
    if ( not mLevel.mScene.HasEntity( selectedEntity ) )
        return;

    ImGui::BeginChild( "Components" );
    if ( Editable() )
    {
        Scene& scene = mLevel.mScene;
        const auto& componentIDs = ComponentManager::Ids();
        const vector<ComponentTypeId> entityComponents = scene.ComponentsOf( selectedEntity );

        // Entity components popups
        if ( ImGui::IsWindowHovered() and ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
            ImGui::OpenPopup( "Entity components popup" );

        if ( ImGui::BeginPopup( "Entity components popup" ) )
        {
            if ( ImGui::BeginMenu( "Add component" ) )
            {
                for ( const auto componentId : componentIDs )
                {
                    // The tree's own: set by moving and instantiating, not by hand.
                    if ( scene.HasComponent( selectedEntity, componentId ) or componentId == HierarchyComponent::ID() or
                         componentId == PrefabInstanceComponent::ID() )
                        continue;

                    const auto name = ComponentManager::GetName( componentId );
                    if ( ImGui::MenuItem( name.data() ) )
                        Invoke( "entity.add_component", { { "entity", (u64)selectedEntity }, { "component", name } } );
                }
                ImGui::EndMenu();
            }

            if ( entityComponents.size() > 1 and // More then tag component
                 ImGui::BeginMenu( "Remove component" ) )
            {
                for ( auto componentID : componentIDs )
                {
                    if ( not scene.HasComponent( selectedEntity, componentID ) or
                         componentID == TagComponent::ID() or // Can't remove tag
                         componentID == HierarchyComponent::ID() )
                        continue;

                    auto name = ComponentManager::GetName( componentID );
                    if ( ImGui::MenuItem( name.data() ) )
                        Invoke( "entity.remove_component", { { "entity", (u64)selectedEntity }, { "component", name } } );
                }
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }

        // Entity components
        InspectorContext ctx{ mProject, mLevel.mScene, mHistory };
        for ( const ComponentTypeId componentID : entityComponents )
        {
            // Looked up again for each: a component drawn before may have
            // changed what the entity has.
            void* componentRaw = scene.TryGetComponent( selectedEntity, componentID );
            if ( not componentRaw )
                continue;
            auto onDrawFunc = ComponentManager::GetOnDraw( componentID );
            if ( onDrawFunc )
                onDrawFunc( ctx, selectedEntity, componentRaw );
            else
                ImGui::Text( "%s", std::format( "Component {} not drawable", componentID ).c_str() );
            ImGui::Separator();
            ImGui::Dummy( ImVec2( 0, 10 ) );
        }
    }
    ImGui::EndChild();
}


void ProjectTreeWindow::OnDraw( DeltaTime )
{
    if ( not mUIGlobals.mShow.mEntities )
        return;
    ImGui::Begin( Name().data(), &mUIGlobals.mShow.mEntities );
    DrawContent();
    ImGui::End();
}

void ProjectTreeWindow::DrawContent()
{
    DrawEntities();
    ImGui::Separator();

    ImGui::Text( "Entity components" );
    ImGui::Separator();
    if ( mSelection.GetEntities().size() == 1 )
        DrawSelectedEntityComponents();
    // The inspector's add / remove component, which walk the pools.
    RunDeferred();
}

}
