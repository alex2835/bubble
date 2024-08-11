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
protected:
    bool mOpen = true;
};
}
