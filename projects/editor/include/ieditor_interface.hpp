#pragma once
#include <string>
#include "engine/utils/types.hpp"
#include "engine/utils/timer.hpp"

namespace bubble
{
struct Engine;
struct EditorState;

class IEditorInterface
{
public:
    IEditorInterface( EditorState& editorState,
                      Engine& engine )
        : mEditorState( editorState ),
          mEngine( engine )
    {}
    virtual ~IEditorInterface()
    {}
    virtual string_view Name() = 0;
    virtual void OnInit() = 0;
    // Called out of ImGui
    virtual void OnUpdate( DeltaTime dt ) = 0;
    virtual void OnDraw( DeltaTime dt ) = 0;
protected:
    EditorState& mEditorState;
    Engine& mEngine;
    bool mOpen = true;
};
}
