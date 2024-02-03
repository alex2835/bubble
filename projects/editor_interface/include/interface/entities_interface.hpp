#pragma once
#include "imgui.h"
#include "engine/utils/ieditor_interface.hpp"
#include "engine/engine.hpp"

namespace bubble
{
class EntitiesInterface : public IEditorInterface
{
public:
    EntitiesInterface()
    {

    }

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


    void DrawEntities( Engine& engine )
    {
        ImGui::BeginChild( "Entities", ImVec2( 0, 400 ) );
        {
            if ( ImGui::Button( "Create Entity", ImVec2( 100, 25 ) ) )
                engine.mScene.CreateEntity();

            ImGui::SameLine();
            if ( ImGui::Button( "Delete Entity", ImVec2( 100, 25 ) ) )
            {
                if ( mSelectedEntity )
                    engine.mScene.RemoveEntity( mSelectedEntity );
                mSelectedEntity = Entity();
            }

            engine.mScene.ForEachEntity( [&]( Entity entity )
            {
                auto& tag = entity.GetComponent<TagComponent>();
                ImGui::Selectable( tag.c_str(), entity == mSelectedEntity );

                if ( ImGui::IsItemClicked() )
                    mSelectedEntity = entity;
            } );
        }
        ImGui::EndChild();
    }

    void DrawSelectedEntityProperties( Engine& engine )
    {
        if ( mSelectedEntity == INVALID_ENTITY )
            return;

        ImGui::BeginChild( "Components" );
        engine.mScene.ForEachEntityComponentRaw( mSelectedEntity,
        [&]( std::string_view componentName, void* componentRaw )
        {
            auto& onDrawStorage = OnComponentDrawFuncStorage::Instance();
            auto onDrawFunc = onDrawStorage.Get( componentName );
            if ( onDrawFunc )
                onDrawFunc( componentRaw );
            else
                ImGui::Text( format( "Component {} not drawable", componentName ).c_str() );
            ImGui::Separator();
        });
        ImGui::EndChild();
    }

    void OnDraw( Engine& engine ) override
    {
        ImGui::Begin( Name().data(), &mOpen );
        {
            DrawEntities( engine );
            ImGui::Separator();
            ImGui::Text( "Entity components" );
            ImGui::Separator();
            DrawSelectedEntityProperties( engine );
        }
        ImGui::End();
    }

private:
    strunoset mNoCompDrawFuncWarnings;
    Entity mSelectedEntity;
};

}
