
#include <imgui.h>
#include "editor_user_interface/windows/entities_window.hpp"
#include "editor_application/editor_application.hpp"
#include "engine/scene/component_manager.hpp"

namespace bubble
{
constexpr auto PROJECT_TREE_NODE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen |
                                         ImGuiTreeNodeFlags_SpanAllColumns |
                                         ImGuiTreeNodeFlags_Framed;

EntitiesWindow::~EntitiesWindow()
{

}

magic_enum::string_view EntitiesWindow::Name()
{
    return "Entities"sv;
}

void EntitiesWindow::OnUpdate( DeltaTime )
{
}


void EntitiesWindow::DrawCreateEntityPopup( Ref<ProjectTreeNode>& node )
{
    // Create entities popup
    if ( ImGui::BeginPopup( "Create entity popup" ) )
    {
        // Temp: creation position. TODO: test raycast
        auto newEntityPos = mSceneCamera.mPosition + mSceneCamera.mForward * 30.0f;

        if ( ImGui::MenuItem( "Create folder" ) )
            node->CreateChild( ProjectTreeNodeType::Folder, "folder"s );

        if ( ImGui::MenuItem( "Create Model Object" ) )
        {
            auto entity = mProject.mScene.CreateEntity();
            mProject.mScene.AddComponent<TagComponent>( entity, "Model object" );
            TransformComponent trans{ .mPosition = newEntityPos };
            mProject.mScene.AddComponent<TransformComponent>( entity, trans );
            mProject.mScene.AddComponent<ModelComponent>( entity );
            mProject.mScene.AddComponent<ShaderComponent>( entity );
            mSelectedEntity = entity;
            node->CreateChild( ProjectTreeNodeType::ModelObject, entity );
        }
        if ( ImGui::MenuItem( "Create Physics Object" ) )
        {
            auto entity = mProject.mScene.CreateEntity();
            mProject.mScene.AddComponent<TagComponent>( entity, "Physics object" );
            TransformComponent trans{ .mPosition = newEntityPos };
            mProject.mScene.AddComponent<TransformComponent>( entity, trans );
            mProject.mScene.AddComponent<ModelComponent>( entity );
            mProject.mScene.AddComponent<ShaderComponent>( entity );
            mProject.mScene.AddComponent<PhysicsComponent>( entity );
            mSelectedEntity = entity;
            node->CreateChild( ProjectTreeNodeType::PhysicsObject, entity );
        }
        if ( ImGui::MenuItem( "Create Game Object" ) )
        {
            auto entity = mProject.mScene.CreateEntity();
            mProject.mScene.AddComponent<TagComponent>( entity, "Game object" );
            TransformComponent trans{ .mPosition = newEntityPos };
            mProject.mScene.AddComponent<TransformComponent>( entity, trans );
            mProject.mScene.AddComponent<ModelComponent>( entity );
            mProject.mScene.AddComponent<ShaderComponent>( entity );
            mProject.mScene.AddComponent<PhysicsComponent>( entity );
            mProject.mScene.AddComponent<StateComponent>( entity );
            mProject.mScene.AddComponent<ScriptComponent>( entity );
            mSelectedEntity = entity;
            node->CreateChild( ProjectTreeNodeType::GameObject, entity );
        }
        if ( ImGui::MenuItem( "Create Script" ) )
        {
            auto entity = mProject.mScene.CreateEntity();
            mProject.mScene.AddComponent<TagComponent>( entity, "Script" );
            mProject.mScene.AddComponent<StateComponent>( entity );
            mProject.mScene.AddComponent<ScriptComponent>( entity );
            mSelectedEntity = entity;
            node->CreateChild( ProjectTreeNodeType::Script, entity );
        }
        ImGui::EndPopup();
    }
}


bool RenamableTreeNode( string& name,
                        bool& editing,
                        ImGuiTreeNodeFlags treeFlags )
{
    constexpr size_t bufferSize = 128;
    static char nameBuffer[bufferSize];

    if ( ImGui::TreeNodeEx( name.c_str(), treeFlags, editing ? "" : name.c_str() ) )
    {
        auto isNodeHovered = ImGui::IsItemHovered();
        if ( isNodeHovered and ImGui::IsKeyPressed( ImGuiKey_F2 ) )
        {
            editing = true;
            strcpy_s( nameBuffer, bufferSize, name.data() );
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

void EntitiesWindow::DrawSceneTreeNode( Ref<ProjectTreeNode>& node )
{
    switch ( node->Type() )
    {
        case ProjectTreeNodeType::Level:
        case ProjectTreeNodeType::Folder:
        {
            string& name = std::get<string>( node->State() );
            if ( RenamableTreeNode( name, node->mEditing, PROJECT_TREE_NODE_FLAGS ) )
            {
                if ( ImGui::IsItemHovered() and ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
                    ImGui::OpenPopup( "Create entity popup" );
                DrawCreateEntityPopup( node );

                for ( auto& child : node->Children() )
                {
                    ImGui::PushID( &child );
                    DrawSceneTreeNode( child );
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
        {
            auto entity = std::get<Entity>( node->State() );
            auto& tag = mProject.mScene.GetComponent<TagComponent>( entity );
            auto displayEntity = std::format( "{} (Enity:{})", tag.mName, (u64)entity );

            ImGui::Selectable( displayEntity.c_str(), entity == mSelectedEntity );
            if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) or 
                 ImGui::IsItemClicked( ImGuiMouseButton_Right ) )
                mSelectedEntity = entity;
        }break;
    }

}

void EntitiesWindow::DrawEntities()
{
    ImGui::BeginChild( "Entities", ImVec2( 0, 400 ) );
    if ( mEditorMode == EditorMode::Editing )
    {
        DrawSceneTreeNode( mProject.mProjectTreeRoot );

        
        //// Entity list
        //Entity entityToDelete;
        //mProject.mScene.ForEachEntity( [&]( Entity entity )
        //{
        //    auto& tag = mProject.mScene.GetComponent<TagComponent>( entity );
        //    auto displayEntity = std::format("{} (id:{})", tag.mName, (u64)entity );
        //    ImGui::Selectable( displayEntity.c_str(), entity == mSelectedEntity );
        //    if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) or
        //         ImGui::IsItemClicked( ImGuiMouseButton_Right ) )
        //        mSelectedEntity = entity;
        //
        //    // Delete entity popup
        //    if ( ImGui::IsItemClicked( ImGuiMouseButton_Right ) )
        //        ImGui::OpenPopup( "Delete entity popup" );
        //    if ( ImGui::BeginPopup( "Delete entity popup" ) )
        //    {
        //        if ( entity == mSelectedEntity and
        //             ImGui::MenuItem( "Delete Entity" ) )
        //        {
        //            entityToDelete = mSelectedEntity;
        //            mSelectedEntity = Entity();
        //        }
        //        ImGui::EndPopup();
        //        ImGui::CloseCurrentPopup();
        //    }
        //} );
        //if ( entityToDelete )
        //    mProject.mScene.RemoveEntity( entityToDelete );




    }
    ImGui::EndChild();
}

void EntitiesWindow::DrawSelectedEntityComponents()
{
    if ( mSelectedEntity == INVALID_ENTITY )
        return;

    ImGui::BeginChild( "Components" );
    if ( mEditorMode == EditorMode::Editing )
    {
        const auto& componentIDs = mProject.mScene.AllComponentTypeIds();
        const auto& entityComponents = mProject.mScene.EntityComponentTypeIds( mSelectedEntity );

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
                        mProject.mScene.EntityAddComponentId( mSelectedEntity, componentID );
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
                        mProject.mScene.EntityRemoveComponentId( mSelectedEntity, componentID );
                }
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }

        // Entity components
        mProject.mScene.ForEachEntityComponentRaw( mSelectedEntity,
                                                   [&]( recs::ComponentTypeId componentID, void* componentRaw )
        {
            auto onDrawFunc = ComponentManager::GetOnDraw( componentID );
            if ( onDrawFunc )
                onDrawFunc( mProject, mSelectedEntity, componentRaw );
            else
                ImGui::Text( std::format( "Component {} not drawable", componentID ).c_str() );
            ImGui::Separator();
            ImGui::Dummy( ImVec2( 0, 10 ) );
        } );
    }
    ImGui::EndChild();
}

void EntitiesWindow::OnDraw( DeltaTime )
{
    ImGui::Begin( Name().data(), &mOpen );
    {
        DrawEntities();
        ImGui::Separator();
        ImGui::Text( "Entity components" );
        ImGui::Separator();
        DrawSelectedEntityComponents();
    }
    ImGui::End();
}

}