#include "engine/pch/pch.hpp"
#include "editor_user_interface/windows/project_tree_window.hpp"
#include "editor_application/editor_application.hpp"
#include "engine/scene/component_manager.hpp"
#include <imgui.h>
#include <cstring>


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
        case ProjectTreeNodeType::Level:
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
    }
    throw std::runtime_error( std::format( "Invalid enum type {}", (u32)node->Type() ) );
}


void ProjectTreeWindow::SetSelectionByNode( const Ref<ProjectTreeNode>& node )
{
    mSelection.SelectTreeNode( node, mProject.mScene );
}


void ProjectTreeWindow::RemoveSelected()
{
    RemoveNodeByEntities( mProject.mProjectTreeRoot, mSelection.GetEntities() );

    if ( mSelection.GetTreeNode() )
        RemoveNode( mProject.mProjectTreeRoot, mSelection.GetTreeNode() );

    for ( auto entity : mSelection.GetEntities() )
        mProject.mScene.RemoveEntity( entity );

    mSelection = {};
}


void ProjectTreeWindow::DrawCreateEntityPopup( Ref<ProjectTreeNode>& node )
{
    // Create entities popup
    if ( ImGui::BeginPopup( "Create entity popup" ) )
    {
        // Temp: creation position. TODO: test raycast
        Transform trans{ .mPosition = mSceneCamera.mPosition + mSceneCamera.mForward * 30.0f };

        if ( ImGui::MenuItem( "Create folder" ) )
        {
            auto child = node->CreateChild( ProjectTreeNodeType::Folder, "folder"s );
            SetSelectionByNode( child );
        }
        if ( ImGui::MenuItem( "Create Model Object" ) )
        {
            auto entity = mProject.mScene.CreateEntity();
            mProject.mScene.AddComponent<TagComponent>( entity, "Model object" );
            mProject.mScene.AddComponent<TransformComponent>( entity, trans );
            mProject.mScene.AddComponent<ModelComponent>( entity );
            mProject.mScene.AddComponent<ShaderComponent>( entity );
            auto child = node->CreateChild( ProjectTreeNodeType::ModelObject, entity );
            SetSelectionByNode( child );
        }
        if ( ImGui::MenuItem( "Create Physics Object" ) )
        {
            auto entity = mProject.mScene.CreateEntity();
            mProject.mScene.AddComponent<TagComponent>( entity, "Physics object" );
            mProject.mScene.AddComponent<TransformComponent>( entity, trans );
            mProject.mScene.AddComponent<ModelComponent>( entity );
            mProject.mScene.AddComponent<ShaderComponent>( entity );
            mProject.mScene.AddComponent<PhysicsComponent>( entity );
            auto child = node->CreateChild( ProjectTreeNodeType::PhysicsObject, entity );
            SetSelectionByNode( child );
        }
        if ( ImGui::MenuItem( "Create Game Object" ) )
        {
            auto entity = mProject.mScene.CreateEntity();
            mProject.mScene.AddComponent<TagComponent>( entity, "Game object" );
            mProject.mScene.AddComponent<TransformComponent>( entity, trans );
            mProject.mScene.AddComponent<ModelComponent>( entity );
            mProject.mScene.AddComponent<ShaderComponent>( entity );
            mProject.mScene.AddComponent<PhysicsComponent>( entity );
            mProject.mScene.AddComponent<StateComponent>( entity );
            mProject.mScene.AddComponent<ScriptComponent>( entity );
            auto child = node->CreateChild( ProjectTreeNodeType::GameObject, entity );
            SetSelectionByNode( child );
        }
        if ( ImGui::MenuItem( "Create Script" ) )
        {
            auto entity = mProject.mScene.CreateEntity();
            mProject.mScene.AddComponent<TagComponent>( entity, "Script" );
            mProject.mScene.AddComponent<StateComponent>( entity );
            mProject.mScene.AddComponent<ScriptComponent>( entity );
            auto child = node->CreateChild( ProjectTreeNodeType::Script, entity );
            SetSelectionByNode( child );
        }
        if ( ImGui::MenuItem( "Create Light" ) )
        {
            auto entity = mProject.mScene.CreateEntity();
            mProject.mScene.AddComponent<TagComponent>( entity, "Light" );
            mProject.mScene.AddComponent<TransformComponent>( entity, trans );
            mProject.mScene.AddComponent<LightComponent>( entity );
            auto child = node->CreateChild( ProjectTreeNodeType::Light, entity );
            SetSelectionByNode( child );
        }
        if ( ImGui::MenuItem( "Create Camera" ) )
        {
            auto entity = mProject.mScene.CreateEntity();
            mProject.mScene.AddComponent<TagComponent>( entity, "Camera" );
            mProject.mScene.AddComponent<TransformComponent>( entity, trans );
            mProject.mScene.AddComponent<CameraComponent>( entity );
            auto child = node->CreateChild( ProjectTreeNodeType::Camera, entity );
            SetSelectionByNode( child );
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
        case ProjectTreeNodeType::Level:
        case ProjectTreeNodeType::Folder:
        {
            ImGui::Dummy( ImVec2( 0, 0 ) );

            ImGui::Image( icon->RendererID(), ImVec2{ 18, 18 } );
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

                for ( auto& child : node->Children() )
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
        {
            ImGui::Image( icon->RendererID(), ImVec2{ 18, 18 } );
            ImGui::SameLine();

            auto entity = std::get<Entity>( node->State() );
            auto& tag = mProject.mScene.GetComponent<TagComponent>( entity );
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
        DrawSceneTreeNode( mProject.mProjectTreeRoot );

        // Remove current selection 
        if ( ( ImGui::IsWindowHovered() or mUIGlobals.mIsViewportHovered ) and
             ImGui::IsKeyDown( ImGuiKey_Delete ) )
        {
            RemoveSelected();
        }
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
        const auto& componentIDs = mProject.mScene.AllComponentTypeIds();
        const auto& entityComponents = mProject.mScene.EntityComponentTypeIds( selectedEntity );

        // Entity components popups
        if ( ImGui::IsWindowHovered() and ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
            ImGui::OpenPopup( "Entity components popup" );

        if ( ImGui::BeginPopup( "Entity components popup" ) )
        {
            if ( ImGui::BeginMenu( "Add component" ) )
            {
                for ( auto componentID : componentIDs )
                {
                    if ( entityComponents.contains( componentID ) )
                        continue;

                    auto name = ComponentManager::GetName( componentID );
                    if ( ImGui::MenuItem( name.data() ) )
                        mProject.mScene.EntityAddComponentId( selectedEntity, componentID );
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
                        mProject.mScene.EntityRemoveComponentId( selectedEntity, componentID );
                }
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }

        // Entity components
        mProject.mScene.ForEachEntityComponentRaw( selectedEntity, 
                                                   [&]( recs::ComponentTypeId componentID, void* componentRaw )
        {
            auto onDrawFunc = ComponentManager::GetOnDraw( componentID );
            if ( onDrawFunc )
                onDrawFunc( mProject, selectedEntity, componentRaw );
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
    ImGui::Begin( Name().data(), &mOpen );
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