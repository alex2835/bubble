#pragma once
#include "imgui.h"
#include "ieditor_interface.hpp"
#include "editor_app.hpp"


namespace bubble
{
class EntitiesInterface : public IEditorInterface
{
public:
    using IEditorInterface::IEditorInterface;

    ~EntitiesInterface() override
    {

    }

    string_view Name() override
    {
        return "Entities";
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
                mEngine.mScene.CreateEntity();

            ImGui::SameLine();
            if ( ImGui::Button( "Delete Entity", ImVec2( 100, 25 ) ) )
            {
                if ( mEditorState.mSelectedEntity )
                    mEngine.mScene.RemoveEntity( mEditorState.mSelectedEntity );
                mEditorState.mSelectedEntity = Entity();
            }

            mEngine.mScene.ForEachEntity( [&]( Entity entity )
            {
                auto& tag = entity.GetComponent<TagComponent>();
                ImGui::Selectable( tag.c_str(), entity == mEditorState.mSelectedEntity );

                if ( ImGui::IsItemClicked() )
                    mEditorState.mSelectedEntity = entity;
            } );
        }
        ImGui::EndChild();
    }

    void DrawSelectedEntityProperties()
    {
        if ( mEditorState.mSelectedEntity == INVALID_ENTITY )
            return;

        ImGui::BeginChild( "Components" );
        mEngine.mScene.ForEachEntityComponentRaw( mEditorState.mSelectedEntity,
        [&]( std::string_view componentName, void* componentRaw )
        {
            auto onDrawFunc = ComponentsOnDrawStorage::Get( componentName );
            if ( onDrawFunc )
                onDrawFunc( componentRaw );
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
    strunoset mNoCompDrawFuncWarnings;
};

}
