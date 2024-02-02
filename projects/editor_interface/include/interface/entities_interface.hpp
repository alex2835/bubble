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
        ImGui::Begin( Name().data(), &mOpen );
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
        ImGui::End();
    }

    void DrawSelectedEntityProperties( Engine& engine )
    {
        if ( mSelectedEntity == INVALID_ENTITY )
            return;

        ImGui::Begin( "Components" );
        engine.mScene.ForEachEntityComponentRaw( mSelectedEntity,
        [&]( std::string_view componentName, void* componentRaw )
        {
            auto* component = (Component*)componentRaw;
            if ( component->mOnDrawFunc )
                component->mOnDrawFunc( componentRaw );
            else
                ImGui::Text( format( "component {} not drawable", componentName ).c_str() );
        });
        ImGui::End();
    }

    void OnDraw( Engine& engine ) override
    {
        DrawEntities( engine );
        DrawSelectedEntityProperties( engine );
    }

private:
    strunoset mNoCompDrawFuncWarnings;
    Entity mSelectedEntity;
};

}
