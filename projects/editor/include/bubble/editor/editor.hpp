#pragma once
#include "bubble/core/log.hpp"
#include "bubble/editor/editor_project.hpp"
#include "bubble/editor/viewport.hpp"

namespace bubble
{
// The editor's window as ImGui draws it, over a project: the dock space, the
// menu, the panels - the viewport, the log. The panels show the active
// document, and act on it through operators alone.
class Editor
{
public:
    // The project, the Gpu and the renderer outlive it.
    Editor( EditorProject& project, Gpu& gpu, Renderer& renderer );
    Editor( const Editor& ) = delete;
    Editor& operator=( const Editor& ) = delete;

    // How the editor looks: its own font - built in, glyphs made as text
    // needs them, Cyrillic too -, at the window's `scale`, ImGui's dark style,
    // docking. Once a context, before its first frame.
    static void SetUpImGui( f32 scale );

    // The frame's widgets, between ImGui::NewFrame and ImGui::Render: the
    // project looked at now and then, the active document's world on a
    // frame, the panels.
    void Frame( f64 elapsed );
    // After them, before ImGui's lists are drawn: what the panels show - the
    // viewport's world.
    void Draw();

    // What the window's title says: the project, the document, a * when it
    // is not saved.
    string Title();
    // Quit was picked.
    bool QuitAsked() const { return mQuitAsked; }
    Viewport& GetViewport() { return mViewport; }

private:
    void Layout( u32 dockSpace );
    void Menu();
    void MenuItem( const char* label, const char* keys, string_view op );
    void Shortcuts();
    void LogPanel();
    // The active document's world, made the first time it is shown.
    OptRef<World> TryActiveWorld();

    EditorProject& mProject;
    Viewport mViewport;
    vector<LogEntry> mLog;
    u64 mLogNext = 0;
    f64 mSinceRefresh = 0;
    bool mLaidOut = false;
    bool mQuitAsked = false;
};
}
