#include "app/editor_interface_loader.hpp"
#include "hot_reloader_import.hpp"
#include <iostream>

namespace bubble
{
EditorInterfaceLoader::EditorInterfaceLoader( ImGuiContext* context )
    : mImGuiContext( context ),
      mHotReloader( CreateRef<hr::HotReloader>( "bubble_interface" ) )
{}

EditorInterfaceLoader::~EditorInterfaceLoader()
{}

void EditorInterfaceLoader::AddInterface( Ref<IEditorInterface> iInterface )
{
    iInterface->OnInit();
    mInterfaces[iInterface->Name()] = iInterface;
}

void EditorInterfaceLoader::LoadInterfaces()
{
    auto contextInit = mHotReloader->GetFunction<void(ImGuiContext*)>( "ImGuiContextInit" );
    contextInit( mImGuiContext );

    auto loader = mHotReloader->GetFunction<void( std::vector<Ref<IEditorInterface>>& )>( "LoadEditorInterface" );
    std::vector<Ref<IEditorInterface>> interfaces;
    loader( interfaces );
    for ( const auto& inter : interfaces )
        AddInterface( inter );
}

void EditorInterfaceLoader::OnUpdate( DeltaTime dt )
{
    if ( mHotReloader->TryUpdate() )
        LoadInterfaces();

    for ( auto& [name, window] : mInterfaces )
        window->OnUpdate( dt );
}

void EditorInterfaceLoader::OnDraw()
{
    for ( auto& [name, window] : mInterfaces )
        window->OnDraw();
}

}
