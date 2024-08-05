#pragma once
#include "editor_interfaces/ieditor_interface.hpp"
#include "editor_application/editor_application.hpp"
#include "engine/utils/imgui_utils.hpp"

namespace bubble
{
auto cFileDialogChooseFileFlags = ImGuiFileBrowserFlags_CloseOnEsc |
                                  ImGuiFileBrowserFlags_ConfirmOnEnter;

auto cFileDialogChooseDirFlags = ImGuiFileBrowserFlags_SelectDirectory |
                                 ImGuiFileBrowserFlags_CloseOnEsc |
                                 ImGuiFileBrowserFlags_CreateNewDir |
                                 ImGuiFileBrowserFlags_ConfirmOnEnter |
                                 ImGuiFileBrowserFlags_HideRegularFiles;

class Menubar : public IEditorInterface
{
public:
    using IEditorInterface::IEditorInterface;
    ~Menubar() override = default;

    string_view Name() override
    {
        return "Menubar"sv;
    }

    void OnInit() override
    {
        mCreateProjectName = "project name"s;
    }

    void OnUpdate( DeltaTime dt ) override
    {

    }

    void ModalCreateProject()
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

                mUINeedUpdateProjectInterface = true;
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

    void ModalOpenProject()
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

                mUINeedUpdateProjectInterface = true;
            }
            catch ( const std::exception& e )
            {
                LogError( e.what() );
            }
        }
    }

    void DrawMenubar()
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

    void OnDraw( DeltaTime dt ) override
    {
        DrawMenubar();

        if ( mCreateProjectModal )
            ModalCreateProject();

        if ( mOpenProjectModal )
            ModalOpenProject();
    }

private:
    ImGui::FileBrowser mFileDialog;
    path mCreateProjectSelectedPath;
    string mCreateProjectName;

    bool mCreateProjectModal = false;
    bool mOpenProjectModal = false;
};

}
