#include "engine/pch/pch.hpp"
#include "editor_user_interface/windows/menubar.hpp"
#include "editor_application/editor_application.hpp"
#include <imgui.h>

namespace bubble
{
constexpr auto cFileDialogChooseFileFlags =
                    ImGuiFileBrowserFlags_CloseOnEsc |
                    ImGuiFileBrowserFlags_ConfirmOnEnter;

constexpr auto cFileDialogChooseDirFlags =
                    ImGuiFileBrowserFlags_SelectDirectory |
                    ImGuiFileBrowserFlags_CloseOnEsc |
                    ImGuiFileBrowserFlags_CreateNewDir |
                    ImGuiFileBrowserFlags_ConfirmOnEnter |
                    ImGuiFileBrowserFlags_HideRegularFiles;


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
        ImGui::InputText( "##menu", mCreateProjectName);

        if ( ImGui::Button( "Browse", ImVec2( 100, 30 ) ) )
        {
            mFileDialog = ImGui::FileBrowser( cFileDialogChooseDirFlags );
            mFileDialog.SetTitle( "Choose directory" );
            mFileDialog.Open();
        }
        ImGui::SameLine();

        ImGui::Text( "%s", mCreateProjectSelectedPath.string().c_str() );

        if ( ImGui::Button( "Create", ImVec2( 100, 30 ) ) )
        {
            mProject.Create( mCreateProjectSelectedPath, mCreateProjectName );
            ImGui::CloseCurrentPopup();
            mCreateProjectModal = false;

            mUIGlobals.mNeedUpdateProjectFilesWindow = true;
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
    if ( not mFileDialog.IsOpened() )
    {
        mFileDialog = ImGui::FileBrowser( cFileDialogChooseFileFlags );
        mFileDialog.SetTypeFilters( { ".bubble" } );
        mFileDialog.Open();
    }

    // File dialog
    mFileDialog.Display();
    if ( not mFileDialog.IsOpened() )
        mOpenProjectModal = false;

    if ( mFileDialog.HasSelected() )
    {
        // Through the editor, which drops the selection and history that
        // point into the project being left, and reports a failed open.
        mUIGlobals.mRequestOpenProject = mFileDialog.GetSelected();
        mFileDialog.ClearSelected();
        mOpenProjectModal = false;
    }
}

void Menubar::ModalNewLevel()
{
    if ( !ImGui::IsPopupOpen( "New level" ) )
        ImGui::OpenPopup( "New level" );

    if ( ImGui::BeginPopupModal( "New level", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
    {
        ImGui::InputText( "##level_name", mNewLevelName );
        ImGui::TextDisabled( "%s", ( mProject.LevelsDir() / mNewLevelName ).replace_extension( LEVEL_FILE_EXT ).string().c_str() );

        if ( ImGui::Button( "Create", ImVec2( 100, 30 ) ) )
        {
            mUIGlobals.mRequestNewLevel = mNewLevelName;
            ImGui::CloseCurrentPopup();
            mNewLevelModal = false;
        }
        ImGui::SameLine( std::max( 200.f, ImGui::GetWindowWidth() - 110 ) );

        if ( ImGui::Button( "Close", ImVec2( 100, 30 ) ) )
        {
            ImGui::CloseCurrentPopup();
            mNewLevelModal = false;
        }
        ImGui::EndPopup();
    }
}

void Menubar::DrawLevelsMenu()
{
    const path current = mProject.CurrentLevel();

    if ( ImGui::MenuItem( "New level..." ) )
        mNewLevelModal = true;

    if ( ImGui::BeginMenu( "Open level" ) )
    {
        const auto levels = mProject.Levels();
        if ( levels.empty() )
            ImGui::TextDisabled( "no levels in %s", mProject.LevelsDir().string().c_str() );

        for ( const auto& level : levels )
        {
            const bool isCurrent = level == current;
            if ( ImGui::MenuItem( level.generic_string().c_str(), nullptr, isCurrent ) and not isCurrent )
            {
                mProject.Save();
                mUIGlobals.mRequestOpenLevel = level;
            }
        }
        ImGui::EndMenu();
    }

    const bool isStartup = current == mProject.mStartupLevel;
    if ( ImGui::MenuItem( "Set as startup level", nullptr, isStartup, not current.empty() ) )
        mProject.mStartupLevel = current;

    ImGui::Separator();
    ImGui::TextDisabled( "startup: %s", mProject.mStartupLevel.generic_string().c_str() );
}

void Menubar::DrawInterfaceMenu()
{
    ImGui::SetNextItemWidth( 200.0f * mWindow.GetUIScale() );
    f32 uiScale = mWindow.GetUIScale();
    if ( ImGui::InputFloat( "Scale", &uiScale, 0.1f, 0.25f, "%.2fx" ) )
        mWindow.SetUIScale( std::clamp( uiScale, 0.5f, 3.0f ) );

    ImGui::SetNextItemWidth( 200.0f * mWindow.GetUIScale() );
    f32 fontSize = mWindow.GetUIFontSize();
    if ( ImGui::InputFloat( "Font size", &fontSize, 1.0f, 2.0f, "%.0f px" ) )
        mWindow.SetUIFontSize( std::clamp( fontSize, 12.0f, 20.0f ) );

    if ( ImGui::MenuItem( "Reset" ) )
    {
        mWindow.SetUIScale( 1.0f );
        mWindow.SetUIFontSize( DEFAULT_UI_FONT_SIZE );
    }

    ImGui::Separator();
    ImGui::TextDisabled( "monitor dpi scale %.2fx", mWindow.GetDPIScale() );

    ImGui::EndMenu();
}

void Menubar::DrawSettingsMenu()
{
    // Settings are written on exit anyway, this is for when you want them kept
    // right now without closing the editor.
    if ( ImGui::MenuItem( "Save editor settings" ) )
    {
        mEditorSettings.Capture( mWindow, mSceneCamera, mUIGlobals );
        mEditorSettings.Save();
        LogInfo( "Editor settings saved: {}", EditorSettings::FilePath().string() );
    }

    if ( ImGui::MenuItem( "Reset editor settings" ) )
    {
        mEditorSettings = EditorSettings();
        mEditorSettings.Apply( mWindow, mSceneCamera, mUIGlobals );
    }

    ImGui::EndMenu();
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

            if ( ImGui::MenuItem( "Save", "Ctrl+S" ) and mProject.IsValid() )
                mProject.Save();

            ImGui::EndMenu();
        }

        if ( mProject.IsValid() and ImGui::BeginMenu( "Level" ) )
        {
            DrawLevelsMenu();
            ImGui::EndMenu();
        }

        if ( ImGui::BeginMenu( "Options" ) )
        {
            if ( ImGui::BeginMenu( "Camera" ) )
            {
                ImGui::EndMenu();
            }

            if ( ImGui::BeginMenu( "Rendering" ) )
            {
                ImGui::Checkbox( "BoundingBoxes", (bool*)&mUIGlobals.mDrawBoundingBoxes );
                ImGui::Checkbox( "PhysicsShapse", (bool*)&mUIGlobals.mDrawPhysicsShapes );
                ImGui::EndMenu();
            }

            if ( ImGui::BeginMenu( "Interface" ) )
                DrawInterfaceMenu();

            if ( ImGui::BeginMenu( "Settings" ) )
                DrawSettingsMenu();

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

    if ( mNewLevelModal )
        ModalNewLevel();
}

}