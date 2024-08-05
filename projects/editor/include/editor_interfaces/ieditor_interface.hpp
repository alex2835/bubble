#pragma once
#include <imgui.h>
#undef APIENTRY
#include <imfilebrowser.h>
#undef min
#undef max

#include <string>
#include "editor_application/editor_state.hpp"
#include "engine/engine.hpp"

namespace bubble
{
struct Engine;

class IEditorInterface : public EditorStateRef
{
public:
    IEditorInterface( EditorState& editorState )
        : EditorStateRef( editorState )
    {}
    virtual ~IEditorInterface()
    {}
    virtual string_view Name() = 0;
    virtual void OnInit() = 0;
    // Called out of ImGui
    virtual void OnUpdate( DeltaTime dt ) = 0;
    virtual void OnDraw( DeltaTime dt ) = 0;
protected:
    bool mOpen = true;
};
}
