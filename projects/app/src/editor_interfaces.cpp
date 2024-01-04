#include "interface/editor_interfaces.hpp"

#include "interface/scnene_viewport_interface.hpp"

namespace bubble
{
EditorInterfaces::EditorInterfaces()
{
    auto viewport = Ref<IEditorInterface>( (IEditorInterface*)new EditorViewportInterface() );
    mInterfaces[viewport->Name()] = viewport;
}

EditorInterfaces::~EditorInterfaces()
{

}

void EditorInterfaces::LoadInterfaces()
{

}

void EditorInterfaces::OnUpdate( DeltaTime dt )
{

}

void EditorInterfaces::OnDraw()
{
    for ( auto& [name, interface] : mInterfaces )
        interface->OnDraw();
}

}
