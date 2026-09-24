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
    : UserInterfaceWindowBase( editor )
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
    }
    throw std::runtime_error( std::format( "Invalid enum type {}", (u32)node->Type() ) );
}


void ProjectTreeWindow::SetSelectionByNode( const Ref<ProjectTreeNode>& node )
{
    mSelection.SelectTreeNode( node, mProject.mLevel.mScene );
}


void ProjectTreeWindow::Invoke( const char* op, const json& args )
{
    try
    {
        OperatorContext ctx = Operators();
        InvokeOperator( op, ctx, args );
    }
    catch ( const std::exception& e )
    {
        LogError( "{}: {}", op, e.what() );
    }
}

void ProjectTreeWindow::DrawCreateEntityPopup( Ref<ProjectTreeNode>& node )
{
    // Create entities popup
    if ( ImGui::BeginPopup( "Create entity popup" ) )
    {
        // Temp: creation position. TODO: test raycast
        const vec3 spawnAt = mSceneCamera.mPosition + mSceneCamera.mForward * 30.0f;

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
        ImGui::EndPopup();
    }
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
        {
            ImGui::Image( (ImTextureID)icon->ImTextureId(), ImVec2{ 18, 18 } );
            ImGui::SameLine();

            auto entity = std::get<Entity>( node->State() );
            auto& tag = mProject.mLevel.mScene.GetComponent<TagComponent>( entity );
            auto displayEntity = std::format( "{} (Enity:{})", tag.mName, (u64)entity );

            ImGui::Selectable( displayEntity.c_str(), isSelected );
            if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) or
                 ImGui::IsItemClicked( ImGuiMouseButton_Right ) )
                SetSelectionByNode( node );
        }break;
    }
}


void ProjectTreeWindow::DrawEntities()
{
    ImGui::BeginChild( "Entities", ImVec2( 0, 400 ), ImGuiChildFlags_ResizeY );
    if ( mEditorMode == EditorMode::Editing )
    {
        DrawSceneTreeNode( mProject.mLevel.mTreeRoot );
    }
    ImGui::EndChild();
}


void ProjectTreeWindow::DrawSelectedEntityComponents()
{
    BUBBLE_ASSERT( mSelection.GetEntities().size() == 1, "Draw only one entity selected" );
    auto selectedEntity = *mSelection.GetEntities().begin();
    if ( selectedEntity == INVALID_ENTITY )
        return;

    ImGui::BeginChild( "Components" );
    if ( mEditorMode == EditorMode::Editing )
    {
        const auto& componentIDs = mProject.mLevel.mScene.AllComponentTypeIds();
        const auto& entityComponents = mProject.mLevel.mScene.EntityComponentTypeIds( selectedEntity );

        // Entity components popups
        if ( ImGui::IsWindowHovered() and ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
            ImGui::OpenPopup( "Entity components popup" );

        if ( ImGui::BeginPopup( "Entity components popup" ) )
        {
            if ( ImGui::BeginMenu( "Add component" ) )
            {
                for ( const auto componentId : componentIDs )
                {
                    if ( entityComponents.contains( componentId ) )
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
                         componentID == TagComponent::ID() ) // Can't remove tag
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
        InspectorContext ctx{ mProject, mHistory };
        mProject.mLevel.mScene.ForEachEntityComponentRaw( selectedEntity, 
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
    {
        DrawEntities();
        ImGui::Separator();

        ImGui::Text( "Entity components" );
        ImGui::Separator();
        if ( mSelection.GetEntities().size() == 1 )
            DrawSelectedEntityComponents();
    }
    ImGui::End();
}

}