
#include "editor_user_interface/windows/menubar.hpp"

namespace bubble
{
Menubar::~Menubar()
{

}

bubble::string_view Menubar::Name()
{
    return "Menubar"sv;
}

void Menubar::OnUpdate( DeltaTime dt )
{

}

void Menubar::ModalCreateProject()
{
    if ( !ImGui::IsPopupOpen( "Create project" ) )
        ImGui::OpenPopup( "Create project" );

    if ( ImGui::BeginPopupModal( "Create project", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
    {
        ImGui::InputText( mCreateProjectName );

        if ( ImGui::Button( "Browse", ImVec2( 100, 30 ) ) )
        {
            mFileDialog = ImGui::FileBrowser( cFileDialogChooseDirFlags );
            mFileDialog.SetTitle( "Choose directory" );
            mFileDialog.Open();
        }
        ImGui::SameLine();

        ImGui::Text( mCreateProjectSelectedPath.string().c_str() );

        if ( ImGui::Button( "Create", ImVec2( 100, 30 ) ) )
        {
            mProject.Create( mCreateProjectSelectedPath, mCreateProjectName );
            ImGui::CloseCurrentPopup();
            mCreateProjectModal = false;

            mUINeedUpdateProjectWindow = true;
        }
        ImGui::SameLine( std::max( 200.f, ImGui::GetWindowWidth() - 110 ) );

        if ( ImGui::Button( "Close", ImVec2( 100, 30 ) ) )
        {
            ImGui::CloseCurrentPopup();
            mCreateProjectModal = false;
        }

        // File dialog
        mFileDialog.Display();
        if ( mFileDialog.HasSelected() )
        {
            mCreateProjectSelectedPath = mFileDialog.GetSelected();
            mFileDialog.ClearSelected();
        }
        ImGui::EndPopup();
    }
}

void Menubar::ModalOpenProject()
{
    if ( !mFileDialog.IsOpened() )
    {
        mFileDialog = ImGui::FileBrowser( cFileDialogChooseFileFlags );
        mFileDialog.SetTypeFilters( { ".bubble" } );
        mFileDialog.Open();
    }

    // File dialog
    mFileDialog.Display();
    if ( mFileDialog.HasSelected() )
    {
        try
        {
            auto projectPath = mFileDialog.GetSelected();
            mFileDialog.ClearSelected();
            mOpenProjectModal = false;
            mProject.Open( projectPath );

            mUINeedUpdateProjectWindow = true;
        }
        catch ( const std::exception& e )
        {
            LogError( e.what() );
        }
    }
}

void Menubar::DrawMenubar()
{
    if ( ImGui::BeginMainMenuBar() )
    {
        if ( ImGui::BeginMenu( "File" ) )
        {
            if ( ImGui::MenuItem( "Create" ) )
                mCreateProjectModal = true;

            if ( ImGui::MenuItem( "Open" ) )
                mOpenProjectModal = true;

            if ( ImGui::MenuItem( "Save" ) )
                mProject.Save();

            ImGui::EndMenu();
        }


        if ( ImGui::BeginMenu( "Options" ) )
        {
            if ( ImGui::BeginMenu( "Camera" ) )
            {
                ImGui::EndMenu();
            }

            // Rendering type
            if ( ImGui::BeginMenu( "Rendering" ) )
            {
                //Renderer::Wareframe(mWireframeOption);
                //
                //// BoundingBox
                //ImGui::Checkbox("BoundingBox", (bool*)&mBoundingBoxOption);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void Menubar::OnDraw( DeltaTime dt )
{
    DrawMenubar();

    if ( mCreateProjectModal )
        ModalCreateProject();

    if ( mOpenProjectModal )
        ModalOpenProject();
}

}