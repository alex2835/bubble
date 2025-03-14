#pragma once
#include <imgui.h>
//#include <imfilebrowser.h>
#include "engine/utils/imgui_utils.hpp"
#include "editor_user_interface/windows/window_base.hpp"

namespace bubble
{
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
    //ImGui::FileBrowser mFileDialog;
    path mCreateProjectSelectedPath;
    string mCreateProjectName = "project name"s;

    bool mCreateProjectModal = false;
    bool mOpenProjectModal = false;
};

}
