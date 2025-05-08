
#include <imgui.h>
#include "editor_user_interface/windows/entities_window.hpp"
#include "editor_application/editor_application.hpp"
#include "engine/scene/component_manager.hpp"

namespace bubble
{

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

void EntitiesWindow::DrawEntities()
{
    ImGui::BeginChild( "Entities", ImVec2( 0, 400 ) );
    {
        // Create entity popup
        if ( ImGui::IsWindowHovered() and ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
            ImGui::OpenPopup( "Create entity popup" );

        if ( ImGui::BeginPopup( "Create entity popup" ) )
        {
            if ( ImGui::MenuItem( "Create Entity" ) )
            {
                auto entity = mProject.mScene.CreateEntity();
                mProject.mScene.AddComponent<TagComponent>( entity, "New entity" );
            }
            ImGui::EndPopup();
        }

        // Entity list
        Entity entityToDelete;
        mProject.mScene.ForEachEntity( [&]( Entity entity )
        {
            auto& tag = mProject.mScene.GetComponent<TagComponent>( entity );
            ImGui::Selectable( ( tag.mName + "##TAG" ).c_str(), entity == mSelectedEntity );
            if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) or
                 ImGui::IsItemClicked( ImGuiMouseButton_Right ) )
                mSelectedEntity = entity;

            // Delete entity popup
            if ( ImGui::IsItemClicked( ImGuiMouseButton_Right ) )
                ImGui::OpenPopup( "Delete entity popup" );
            if ( ImGui::BeginPopup( "Delete entity popup" ) )
            {
                if ( entity == mSelectedEntity and
                     ImGui::MenuItem( "Delete Entity" ) )
                {
                    entityToDelete = mSelectedEntity;
                    mSelectedEntity = Entity();
                }
                ImGui::EndPopup();
                ImGui::CloseCurrentPopup();
            }
        } );
        if ( entityToDelete )
            mProject.mScene.RemoveEntity( entityToDelete );
    }
    ImGui::EndChild();
}

void EntitiesWindow::DrawSelectedEntityComponents()
{
    if ( mSelectedEntity == INVALID_ENTITY )
        return;

    ImGui::BeginChild( "Components" );
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