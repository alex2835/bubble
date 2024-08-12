#pragma once
#include "engine/utils/imgui_utils.hpp"
#include "editor_user_interface/windows/window_base.hpp"

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

class Menubar : public UserInterfaceWindowBase
{
public:
    using UserInterfaceWindowBase::UserInterfaceWindowBase;
    ~Menubar();

    string_view Name();

    void OnUpdate( DeltaTime dt );
    void ModalCreateProject();
    void ModalOpenProject();
    void DrawMenubar();
    void OnDraw( DeltaTime dt );

private:
    ImGui::FileBrowser mFileDialog;
    path mCreateProjectSelectedPath;
    string mCreateProjectName = "project name"s;

    bool mCreateProjectModal = false;
    bool mOpenProjectModal = false;
};

}
