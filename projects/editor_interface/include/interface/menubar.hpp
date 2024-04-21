#pragma once
#include "ieditor_interface.hpp"
#include "editor_application.hpp"

namespace bubble
{
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

    }

    void OnUpdate( DeltaTime dt ) override
    {

    }

    void CreateProjectModal()
    {
        if ( !ImGui::IsPopupOpen( "Create project" ) )
            ImGui::OpenPopup( "Create project" );

        if ( ImGui::BeginPopupModal( "Create project", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
        {
            if ( ImGui::Button( "Browse", ImVec2( 100, 30 ) ) )
            {
                mFileDialog = ImGui::FileBrowser( ImGuiFileBrowserFlags_SelectDirectory |
                                                  ImGuiFileBrowserFlags_CloseOnEsc | 
                                                  ImGuiFileBrowserFlags_CreateNewDir | 
                                                  ImGuiFileBrowserFlags_ConfirmOnEnter |
                                                  ImGuiFileBrowserFlags_HideRegularFiles );
                mFileDialog.SetTitle( "Choose directory" );
                mFileDialog.Open();
            }
            ImGui::SameLine();
            ImGui::Text( mCreateProjectSelectedPath.string().c_str() );

            mFileDialog.Display();
            if ( mFileDialog.HasSelected() )
            {
                mCreateProjectSelectedPath = mFileDialog.GetSelected();
                mFileDialog.ClearSelected();
            }

            if ( ImGui::Button( "Create", ImVec2( 100, 30 ) ) )
            {
                ImGui::CloseCurrentPopup();
                mCreateProjectModal = false;
            }
            ImGui::SameLine( std::max( 200.f, ImGui::GetWindowWidth() - 110 ) );
            if ( ImGui::Button( "Close", ImVec2( 100, 30 ) ) )
            {
                ImGui::CloseCurrentPopup();
                mCreateProjectModal = false;
            }
            ImGui::EndPopup();
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
                {
                    try
                    {
                        path projectPath;
                        mProject.Open( projectPath );
                    }
                    catch ( const std::exception& e )
                    {
                        LogError( e.what() );
                    }
                }

                if ( ImGui::MenuItem( "Save" ) )
                {
                }

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
            CreateProjectModal();
    }

private:
    ImGui::FileBrowser mFileDialog;
    path mCreateProjectSelectedPath;

    bool mCreateProjectModal = false;
    bool mOpenProjectModal = false;
};

}
