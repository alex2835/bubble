#include "engine/pch/pch.hpp"
#include "editor_user_interface/windows/project_tree_window.hpp"
#include "editor_application/editor_application.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/editing/operators/operator.hpp"
#include "engine/serialization/types_serialization.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include <imgui.h>
#include <cstring>
#include "engine/scene/components/tag_component.hpp"
#include "engine/scene/components/hierarchy_component.hpp"
#include "engine/scene/components/prefab_instance_component.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/editing/commands/tree_commands.hpp"


namespace bubble
{
constexpr auto PROJECT_TREE_NODE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen |
                                         ImGuiTreeNodeFlags_SpanAllColumns |
                                         ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                         ImGuiTreeNodeFlags_OpenOnArrow;

constexpr auto SELECTED_PROJECT_TREE_NODE_FLAGS = PROJECT_TREE_NODE_FLAGS | ImGuiTreeNodeFlags_Framed;

bool RenamableTreeNode( string& name,
                        bool& editing,
                        ImGuiTreeNodeFlags treeFlags )
{
    constexpr size_t bufferSize = 128;
    static char nameBuffer[bufferSize];

    if ( ImGui::TreeNodeEx( name.c_str(), treeFlags, "%s", editing ? "" : name.c_str() ) )
    {
        auto isNodeHovered = ImGui::IsItemHovered();
        if ( isNodeHovered and ImGui::IsKeyPressed( ImGuiKey_F2 ) )
        {
            editing = true;
            std::strncpy( nameBuffer, name.data(), bufferSize );
            ImGui::SetKeyboardFocusHere();
        }

        if ( editing )
        {
            ImGui::SameLine();
            auto inputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
            if ( ImGui::InputText( "##rename", nameBuffer, bufferSize, inputFlags ) )
            {
                name = nameBuffer;
                editing = false;
            }
            auto isInputTextHovered = ImGui::IsItemHovered();
            if ( not isInputTextHovered and not isNodeHovered )
                editing = false;
        }
        return true;
    }
    editing = false;
    return false;
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

magic_enum::string_view ProjectTreeWindow::Name()
{
    return "Entities"sv;
}

void ProjectTreeWindow::OnUpdate( DeltaTime )
{
}

const Ref<Texture2D>& ProjectTreeWindow::GetProjectTreeNodeIcon( const Ref<ProjectTreeNode>& node )
{
    switch ( node->Type() )
    {
        case ProjectTreeNodeType::Root:
            return mLevelIcon;
        case ProjectTreeNodeType::Folder:
            return mFolerIcon;
        case ProjectTreeNodeType::ModelObject:
            return mObjectIcon;
        case ProjectTreeNodeType::PhysicsObject:
            return mPhysicsObjectIcon;
        case ProjectTreeNodeType::GameObject:
            return mPlayerIcon;
        case ProjectTreeNodeType::Camera:
            return mCameraIcon;
        case ProjectTreeNodeType::Light:
            return mLightIcon;
        case ProjectTreeNodeType::Script:
            return mScriptIcon;
        case ProjectTreeNodeType::Audio:
            return mAudioIcon;
        case ProjectTreeNodeType::Prefab:
            return mObjectIcon;
    }
    throw std::runtime_error( std::format( "Invalid enum type {}", (u32)node->Type() ) );
}


void ProjectTreeWindow::SetSelectionByNode( const Ref<ProjectTreeNode>& node )
{
    mSelection.SelectTreeNode( node, mLevel.mScene );
}


// Deferred to the end of DrawEntities: most of these change the tree, and
// the tree is being walked - by reference, down vectors a move or a delete
// would shift - while they are asked for.
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

void ProjectTreeWindow::DrawCreateEntityPopup( Ref<ProjectTreeNode>& node )
{
    // Create entities popup
    if ( ImGui::BeginPopup( "Create entity popup" ) )
    {
        const vec3 spawnAt = SpawnPoint();

        // What each kind starts with is the operator's business; the menu only
        // names the kinds.
        static constexpr std::pair<const char*, ProjectTreeNodeType> kinds[] = {
            { "Create folder",         ProjectTreeNodeType::Folder },
            { "Create Model Object",   ProjectTreeNodeType::ModelObject },
            { "Create Physics Object", ProjectTreeNodeType::PhysicsObject },
            { "Create Game Object",    ProjectTreeNodeType::GameObject },
            { "Create Script",         ProjectTreeNodeType::Script },
            { "Create Light",          ProjectTreeNodeType::Light },
            { "Create Camera",         ProjectTreeNodeType::Camera },
            { "Create Audio",          ProjectTreeNodeType::Audio },
        };
        for ( const auto& [label, type] : kinds )
        {
            if ( ImGui::MenuItem( label ) )
                Invoke( "scene.create_node", { { "type", magic_enum::enum_name( type ) },
                                               { "parent", node->ID() },
                                               { "spawn_at", spawnAt } } );
        }
        DrawPrefabMenu( node );
        ImGui::EndPopup();
    }
}

vec3 ProjectTreeWindow::SpawnPoint() const
{
    // TODO: raycast into the scene.
    return mEditableWhileRunning ? vec3( 0 ) : mSceneCamera.mPosition + mSceneCamera.mForward * 30.0f;
}

void ProjectTreeWindow::DrawPrefabMenu( const Ref<ProjectTreeNode>& node )
{
    if ( node->Type() == ProjectTreeNodeType::Root )
        return;
    ImGui::Separator();
    if ( ImGui::BeginMenu( "Save as prefab" ) )
    {
        if ( mPrefabName.empty() )
            mPrefabName = std::format( "prefabs/{}", node->IsEntity() and mLevel.mScene.HasComponent<TagComponent>( node->AsEntity() )
                                                     ? mLevel.mScene.GetComponent<TagComponent>( node->AsEntity() ).mName
                                                     : std::get<string>( node->State() ) );
        ImGui::SetNextItemWidth( 220.0f * mWindow.GetUIScale() );
        ImGui::InputText( "##prefab", mPrefabName );
        ImGui::SameLine();
        if ( ImGui::Button( "Save" ) and not mPrefabName.empty() )
        {
            Invoke( "prefab.save", { { "file", mPrefabName }, { "node", node->ID() } } );
            mUIGlobals.mNeedUpdateProjectFilesWindow = true;
            mPrefabName.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::TextDisabled( "Relative to the project; .prefab is added." );
        ImGui::EndMenu();
    }

    const auto entity = node->TryGetEntity();
    if ( not entity or not mLevel.mScene.HasComponent<PrefabInstanceComponent>( *entity ) )
        return;
    const string prefab = mLevel.mScene.GetComponent<PrefabInstanceComponent>( *entity ).mPrefab;
    if ( ImGui::MenuItem( std::format( "Open {}", prefab ).c_str() ) )
        mOperatorQueue.Enqueue( "prefab.edit", { { "file", prefab } } );
    if ( ImGui::MenuItem( "Update from prefab" ) )
        Invoke( "prefab.update_instances", { { "file", prefab } } );
}


void ProjectTreeWindow::DrawSceneTreeNode( Ref<ProjectTreeNode>& node, bool isSelected )
{
    // Selected by parent / selected in project tree / entities that were selected on screen
    isSelected = isSelected or 
                mSelection.GetTreeNode() == node or
                ( node->IsEntity() and mSelection.GetEntities().contains( node->AsEntity() ) );

    const auto& icon = GetProjectTreeNodeIcon( node );
    switch ( node->Type() )
    {
        case ProjectTreeNodeType::Root:
        case ProjectTreeNodeType::Folder:
        {
            ImGui::Dummy( ImVec2( 0, 0 ) );

            ImGui::Image( (ImTextureID)icon->ImTextureId(), ImVec2{ 18, 18 } );
            ImGui::SameLine();

            string& name = std::get<string>( node->State() );
            const auto flags = isSelected ? SELECTED_PROJECT_TREE_NODE_FLAGS : PROJECT_TREE_NODE_FLAGS;
            if ( RenamableTreeNode( name, node->mIsEditingInUI, flags ) )
            {
                if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) or
                     ImGui::IsItemClicked( ImGuiMouseButton_Right ) )
                    SetSelectionByNode( node );
                DragDropNode( node );

                if ( ImGui::IsItemHovered() and ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
                    ImGui::OpenPopup( "Create entity popup" );
                DrawCreateEntityPopup( node );

                for ( auto& child : node->mChildren )
                {
                    ImGui::PushID( &child );
                    DrawSceneTreeNode( child, isSelected );
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
        }break;
        case ProjectTreeNodeType::ModelObject:
        case ProjectTreeNodeType::PhysicsObject:
        case ProjectTreeNodeType::GameObject:
        case ProjectTreeNodeType::Camera:
        case ProjectTreeNodeType::Script:
        case ProjectTreeNodeType::Light:
        case ProjectTreeNodeType::Audio:
        case ProjectTreeNodeType::Prefab:
        {
            ImGui::Image( (ImTextureID)icon->ImTextureId(), ImVec2{ 18, 18 } );
            ImGui::SameLine();

            auto entity = std::get<Entity>( node->State() );
            auto& tag = mLevel.mScene.GetComponent<TagComponent>( entity );
            auto displayEntity = std::format( "{} (Enity:{}){}", tag.mName, (u64)entity,
                                              mLevel.mScene.HasComponent<PrefabInstanceComponent>( entity ) ? "  [prefab]" : "" );

            // An entity with children is a tree node of its own; one without
            // is a leaf, as before.
            const bool hasChildren = not node->mChildren.empty();
            const auto flags = ( isSelected ? SELECTED_PROJECT_TREE_NODE_FLAGS : PROJECT_TREE_NODE_FLAGS ) |
                               ( hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen );
            const bool open = ImGui::TreeNodeEx( (void*)node.get(), flags, "%s", displayEntity.c_str() );
            if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) or
                 ImGui::IsItemClicked( ImGuiMouseButton_Right ) )
                SetSelectionByNode( node );
            DragDropNode( node );

            if ( ImGui::IsItemHovered() and ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
                ImGui::OpenPopup( "Create entity popup" );
            DrawCreateEntityPopup( node );

            if ( open and hasChildren )
            {
                for ( auto& child : node->mChildren )
                {
                    ImGui::PushID( &child );
                    // Selecting an entity does not select what is under it.
                    DrawSceneTreeNode( child, false );
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
        }break;
    }
}


// Drag a node onto another to put it under it: for an entity onto an
// entity, that is parenting, and the one dragged keeps its place in the
// world. Onto the root, it goes back to the top.
void ProjectTreeWindow::DragDropNode( const Ref<ProjectTreeNode>& node )
{
    if ( node->Type() != ProjectTreeNodeType::Root and ImGui::BeginDragDropSource() )
    {
        const u64 id = node->ID();
        ImGui::SetDragDropPayload( "TREE_NODE", &id, sizeof( id ) );
        ImGui::Text( "%s", node->IsEntity() ? "entity" : std::get<string>( node->State() ).c_str() );
        ImGui::EndDragDropSource();
    }
    if ( ImGui::BeginDragDropTarget() )
    {
        // A prefab from the Project window: an instance under this node.
        if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( "PREFAB_FILE" ) )
            Invoke( "prefab.instantiate", { { "file", string( (const char*)payload->Data ) },
                                            { "parent", node->ID() },
                                            { "spawn_at", SpawnPoint() } } );
        if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( "TREE_NODE" ) )
        {
            const u64 id = *(const u64*)payload->Data;
            auto dragged = FindNodeById( id, mLevel.mTreeRoot );
            if ( dragged and dragged != node and not IsInSubtree( node, dragged ) and dragged->mParent.lock() != node )
                mDeferred.push_back( [this, dragged, target = node]()
                {
                    mHistory.Execute( CreateScope<MoveNodeCommand>( dragged, target, mLevel.mScene ) );
                } );
        }
        ImGui::EndDragDropTarget();
    }
}

void ProjectTreeWindow::DrawEntities()
{
    ImGui::BeginChild( "Entities", ImVec2( 0, 400 ), ImGuiChildFlags_ResizeY );
    if ( Editable() )
    {
        DrawSceneTreeNode( mLevel.mTreeRoot );
    }
    ImGui::EndChild();
    RunDeferred();
}


void ProjectTreeWindow::DrawSelectedEntityComponents()
{
    BUBBLE_ASSERT( mSelection.GetEntities().size() == 1, "Draw only one entity selected" );
    auto selectedEntity = *mSelection.GetEntities().begin();
    if ( selectedEntity == INVALID_ENTITY )
        return;

    ImGui::BeginChild( "Components" );
    if ( Editable() )
    {
        const auto& componentIDs = mLevel.mScene.AllComponentTypeIds();
        const auto& entityComponents = mLevel.mScene.EntityComponentTypeIds( selectedEntity );

        // Entity components popups
        if ( ImGui::IsWindowHovered() and ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
            ImGui::OpenPopup( "Entity components popup" );

        if ( ImGui::BeginPopup( "Entity components popup" ) )
        {
            if ( ImGui::BeginMenu( "Add component" ) )
            {
                for ( const auto componentId : componentIDs )
                {
                    // The hierarchy is set in the tree, not added by hand.
                    if ( entityComponents.contains( componentId ) or componentId == HierarchyComponent::ID() or
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
                    if ( not entityComponents.contains( componentID ) or
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
        mLevel.mScene.ForEachEntityComponentRaw( selectedEntity, 
                                                   [&]( recs::ComponentTypeId componentID, void* componentRaw )
        {
            auto onDrawFunc = ComponentManager::GetOnDraw( componentID );
            if ( onDrawFunc )
                onDrawFunc( ctx, selectedEntity, componentRaw );
            else
                ImGui::Text( "%s", std::format( "Component {} not drawable", componentID ).c_str() );
            ImGui::Separator();
            ImGui::Dummy( ImVec2( 0, 10 ) );
        } );
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