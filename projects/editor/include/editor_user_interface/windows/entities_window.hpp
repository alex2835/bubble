#pragma once
#include "engine/types/set.hpp"
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
    void OnDraw( DeltaTime );

private:
    void DrawSelectedEntityComponents();
    void DrawEntities();
};

}
