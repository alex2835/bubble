#pragma once
#include "editor_interfaces/ieditor_interface.hpp"
#include "engine/scene/components_manager.hpp"

namespace bubble
{
class EntitiesInterface : public IEditorInterface
{
public:
    using IEditorInterface::IEditorInterface;
    ~EntitiesInterface();

    string_view Name();
    void OnUpdate( DeltaTime );
    void DrawEntities();
    void DrawSelectedEntityProperties();
    void OnDraw( DeltaTime );

private:
    str_hash_set mNoCompDrawFuncWarnings;
};

}
