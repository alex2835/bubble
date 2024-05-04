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
            if ( ImGui::Button( "Create Entity", ImVec2( 100, 25 ) ) )
            {
                auto entity = mProject.mScene.CreateEntity();
                entity.AddComponet<TagComponent>( "New entity" );
            }

            ImGui::SameLine();
            if ( ImGui::Button( "Delete Entity", ImVec2( 100, 25 ) ) )
            {
                if ( mSelectedEntity )
                    mProject.mScene.RemoveEntity( mSelectedEntity );
                mSelectedEntity = Entity();
            }

            mProject.mScene.ForEachEntity( [&]( Entity entity )
            {
                auto& tag = entity.GetComponent<TagComponent>();
                ImGui::Selectable(  ( tag + "##TAG" ).c_str(), entity == mSelectedEntity);
                if ( ImGui::IsItemClicked() )
                    mSelectedEntity = entity;
            } );
        }
        ImGui::EndChild();
    }

    void DrawSelectedEntityProperties()
    {
        if ( mSelectedEntity == INVALID_ENTITY )
            return;

        ImGui::BeginChild( "Components" );
        mProject.mScene.ForEachEntityComponentRaw( mSelectedEntity,
        [&]( std::string_view componentName, void* componentRaw )
        {
            auto onDrawFunc = ComponentManager::GetOnDraw( componentName );
            if ( onDrawFunc )
                onDrawFunc( mProject.mLoader, componentRaw );
            else
                ImGui::Text( format( "Component {} not drawable", componentName ).c_str() );
            ImGui::Separator();
        });
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
