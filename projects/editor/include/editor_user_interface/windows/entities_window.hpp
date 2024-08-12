#pragma once
#include "engine/scene/components_manager.hpp"
#include "editor_user_interface/windows/window_base.hpp"

namespace bubble
{
class EntitiesWindow : public UserInterfaceWindowBase
{
public:
    using UserInterfaceWindowBase::UserInterfaceWindowBase;
    ~EntitiesWindow();

    string_view Name();
    void OnUpdate( DeltaTime );
    void DrawEntities();
    void DrawSelectedEntityProperties();
    void OnDraw( DeltaTime );

private:
    str_hash_set mNoCompDrawFuncWarnings;
};

}
