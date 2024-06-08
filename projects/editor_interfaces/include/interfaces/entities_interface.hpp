#pragma once
#include "ieditor_interface.hpp"
#include "editor_application.hpp"

namespace bubble
{
class EntitiesInterface : public IEditorInterface
{
public:
    using IEditorInterface::IEditorInterface;
    ~EntitiesInterface() override = default;

    string_view Name() override
    {
        return "Entities"sv;
    }

    void OnInit() override
    {

    }

    void OnUpdate( DeltaTime ) override
    {

    }

    void DrawEntities()
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
                    entity.AddComponet<TagComponent>( "New entity" );
                }
                ImGui::EndPopup();
            }

            // Entity list
            Entity entityToDelete;
            mProject.mScene.ForEachEntity( [&]( Entity entity )
            {
                auto& tag = entity.GetComponent<TagComponent>();
                ImGui::Selectable( ( tag + "##TAG" ).c_str(), entity == mSelectedEntity );
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

    void DrawSelectedEntityProperties()
    {
        if ( mSelectedEntity == INVALID_ENTITY )
            return;

        ImGui::BeginChild( "Components" );
        {
            const auto& entityComponents = mProject.mScene.mEntitiesComponentIds[mSelectedEntity];

            // Entity components popups
            if ( ImGui::IsWindowHovered() and ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
                ImGui::OpenPopup( "Entity components popup" );

            if ( ImGui::BeginPopup( "Entity components popup" ) )
            {
                if ( ImGui::BeginMenu( "Add component" ) )
                {
                    for ( const auto& [name, id] : mProject.mScene.mComponents )
                    {
                        if ( entityComponents.contains( id ) )
                            continue;

                        if ( ImGui::MenuItem( name.c_str() ) )
                        {
                            auto& pool = mProject.mScene.mPools[id];
                            pool.PushEmpty( mSelectedEntity );
                            mProject.mScene.mEntitiesComponentIds[mSelectedEntity].insert( id );
                        }
                    }
                    ImGui::EndMenu();
                }

                if ( ImGui::BeginMenu( "Remove component" ) )
                {
                    for ( const auto& [name, id] : mProject.mScene.mComponents )
                    {
                        if ( not entityComponents.contains( id ) )
                            continue;

                        if ( ImGui::MenuItem( name.c_str() ) )
                        {
                            auto& pool = mProject.mScene.mPools[id];
                            pool.Remove( mSelectedEntity );
                            mProject.mScene.mEntitiesComponentIds[mSelectedEntity].erase( id );
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }

            // Entity components
            mProject.mScene.ForEachEntityComponentRaw( mSelectedEntity,
            [&]( std::string_view componentName, void* componentRaw )
            {
                auto onDrawFunc = ComponentManager::GetOnDraw( componentName );
                if ( onDrawFunc )
                    onDrawFunc( mProject.mLoader, componentRaw );
                else
                    ImGui::Text( format( "Component {} not drawable", componentName ).c_str() );
                ImGui::Separator();
            } );
        }
        ImGui::EndChild();
    }
    
    void OnDraw( DeltaTime ) override
    {
        ImGui::Begin( Name().data(), &mOpen );
        {
            DrawEntities();
            ImGui::Separator();
            ImGui::Text( "Entity components" );
            ImGui::Separator();
            DrawSelectedEntityProperties();
        }
        ImGui::End();
    }

private:
    str_hash_set mNoCompDrawFuncWarnings;
};

}
