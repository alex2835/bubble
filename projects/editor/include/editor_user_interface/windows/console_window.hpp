#pragma once
#include "editor_user_interface/windows/window_base.hpp"

namespace bubble
{
class EditorLua;

// A line in, the editor's Lua runs it, whatever it printed comes out. The
// place to try an operator by hand before putting it in a script.
class ConsoleWindow : public UserInterfaceWindowBase
{
public:
    ConsoleWindow( BubbleEditor& editor );

    string_view Name();
    void OnUpdate( DeltaTime dt );
    void OnDraw( DeltaTime dt );

private:
    EditorLua& mLua;
    string mInput;
    vector<string> mEntered; // for Up / Down
    int mRecall = -1;
    bool mScrollToBottom = false;
    bool mFocusInput = false;
};

}
