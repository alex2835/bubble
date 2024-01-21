#include "app/editor_interfaces.hpp"
//#include "app/scnene_viewport_interface.hpp"
#include "hot_reloader_import.hpp"
#include <iostream>

namespace bubble
{
EditorInterfaces::EditorInterfaces()
    : mHotReloader( CreateRef<hr::HotReloader>( "bubble_interface" ) )
{

}

EditorInterfaces::~EditorInterfaces()
{

}

void EditorInterfaces::AddInterface( Ref<IEditorInterface> window )
{
    window->OnInit();
    mInterfaces[window->Name()] = window;
}

void EditorInterfaces::LoadInterfaces()
{
    for ( const auto& [name, func] : mHotReloader->GetLibraryMeta() )
        std::cout << "name:" << name << " signature:" << func.ToString() << std::endl;
}

void EditorInterfaces::OnUpdate( DeltaTime dt )
{
    if ( mHotReloader->TryUpdate() )
    {
        auto func = mHotReloader->GetFunction<int( int, int )>( "mul" );
        std::cout << "mul: 2 * 2 = " << func( 2, 2 ) << std::endl;
    }

    for ( auto& [name, window] : mInterfaces )
        window->OnUpdate( dt );
}

void EditorInterfaces::OnDraw()
{
    for ( auto& [name, window] : mInterfaces )
        window->OnDraw();
}

}
