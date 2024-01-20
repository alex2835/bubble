#include "interface/editor_interfaces.hpp"

#include "interface/scnene_viewport_interface.hpp"

namespace bubble
{
EditorInterfaces::EditorInterfaces()
{

}

EditorInterfaces::~EditorInterfaces()
{

}

void EditorInterfaces::AddInterface( Ref<IEditorInterface> interface )
{
    interface->OnInit();
    mInterfaces[interface->Name()] = interface;
}

void EditorInterfaces::LoadInterfaces()
{

}

void EditorInterfaces::OnUpdate( DeltaTime dt )
{
    for ( auto& [name, interface] : mInterfaces )
        interface->OnUpdate( dt );
}

void EditorInterfaces::OnDraw()
{
    for ( auto& [name, interface] : mInterfaces )
        interface->OnDraw();
}

}
