
#include <iostream>
#include "editor_application.hpp"
#undef APIENTRY
#include "ieditor_interface.hpp"
#include "editor_interface_loader.hpp"
#include "hot_reloader_import.hpp"

namespace bubble
{
EditorInterfaceLoader::EditorInterfaceLoader( EditorState& editorState,
                                              Engine& engine,
                                              ImGuiContext* context )
    : mImGuiContext( context ),
      mEditorState( editorState ),
      mEngine( engine ),
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

    auto loader = mHotReloader->GetFunction<void( EditorState&, Engine&, std::vector<Ref<IEditorInterface>>& )>( "LoadEditorInterface" );
    std::vector<Ref<IEditorInterface>> interfaces;
    loader( mEditorState, mEngine, interfaces );
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

void EditorInterfaceLoader::OnDraw( DeltaTime dt )
{
    for ( auto& [name, window] : mInterfaces )
        window->OnDraw( dt );
}

}
