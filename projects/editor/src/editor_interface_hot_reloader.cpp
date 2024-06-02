
#include <iostream>
#include "editor_application.hpp"
#undef APIENTRY
#include "ieditor_interface.hpp"
#include "editor_interface_hot_reloader.hpp"
#include "hot_reloader_import.hpp"

namespace bubble
{
EditorInterfaceHotReloader::EditorInterfaceHotReloader( EditorState& editorState,
                                              Engine& engine )
    : mImGuiContext( editorState.mWindow.GetImGuiContext() ),
      mEditorState( editorState ),
      mEngine( engine ),
      mHotReloader( CreateRef<hr::HotReloader>( "bubble_interface" ) )
{}

EditorInterfaceHotReloader::~EditorInterfaceHotReloader()
{}

void EditorInterfaceHotReloader::AddInterface( Ref<IEditorInterface> editorInterface )
{
    editorInterface->OnInit();
    mInterfaces[editorInterface->Name()] = editorInterface;
}

void EditorInterfaceHotReloader::LoadInterfaces()
{
    auto contextInit = mHotReloader->GetFunction<void(ImGuiContext*)>( "ImGuiContextInit" );
    contextInit( mImGuiContext );

    auto loader = mHotReloader->GetFunction<void( EditorState&, Engine&, std::vector<Ref<IEditorInterface>>& )>( "LoadEditorInterface" );
    std::vector<Ref<IEditorInterface>> interfaces;
    loader( mEditorState, mEngine, interfaces );
    for ( const auto& inter : interfaces )
        AddInterface( inter );
}

void EditorInterfaceHotReloader::OnUpdate( DeltaTime dt )
{
    if ( mHotReloader->TryUpdate() )
        LoadInterfaces();

    for ( auto& [name, window] : mInterfaces )
        window->OnUpdate( dt );
}

void EditorInterfaceHotReloader::OnDraw( DeltaTime dt )
{
    for ( auto& [name, window] : mInterfaces )
        window->OnDraw( dt );
}

}
