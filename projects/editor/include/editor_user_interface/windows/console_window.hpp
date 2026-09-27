#pragma once
#include "editor_user_interface/windows/window_base.hpp"
#include "engine/log/log.hpp"

namespace bubble
{
class EditorLua;

// The engine's log - everything logged and everything scripts print, the
// game's while it runs included - and under it a line for the editor's Lua:
// what it prints comes out in the same place. Filtered by level and text.
class ConsoleWindow : public UserInterfaceWindowBase
{
public:
    ConsoleWindow( BubbleEditor& editor );

    string_view Name();
    void OnUpdate( DeltaTime dt );
    void OnDraw( DeltaTime dt );

private:
    // One row of the view: an entry of several lines is several rows, so
    // every row is one text line high and the list can be clipped.
    struct Row
    {
        LogLevel mLevel;
        string mText;
    };
    void Pull();
    bool Shown( const Row& row ) const;
    void DrawToolbar();
    void DrawLog();
    void DrawInput();

    EditorLua& mLua;
    vector<Row> mRows;
    u64 mNextIndex = 0;
    // Which levels show: Info, Warning, Error, Script.
    bool mShowLevel[4] = { true, true, true, true };
    string mFilter;
    bool mFollow = true;

    string mInput;
    vector<string> mEntered; // for Up / Down
    int mRecall = -1;
    bool mFocusInput = false;
};

}
