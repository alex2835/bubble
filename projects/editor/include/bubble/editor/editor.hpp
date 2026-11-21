#pragma once
#include "bubble/core/log.hpp"
#include "bubble/editor/assets_panel.hpp"
#include "bubble/editor/document_world.hpp"
#include "bubble/editor/editor_project.hpp"
#include "bubble/editor/inspector.hpp"
#include "bubble/editor/scene_panel.hpp"
#include "bubble/editor/viewport.hpp"

namespace bubble
{
// The editor's window as ImGui draws it, over a project: the dock space, the
// menu, the panels - the scene's tree, the viewport with a tab a document,
// the inspector, the project's assets, the log. The panels show the active
// document - a level or a prefab, a material, an animation graph -, and act
// on it through operators alone; a tab picked makes its document the active
// one.
class Editor
{
public:
    // The project, the Gpu and the renderer outlive it; `open` hands a file
    // to the system, as the platform does.
    Editor( EditorProject& project, Gpu& gpu, Renderer& renderer, function<void( const OsPath& )> open );
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
    void DocumentTabs();
    void UnsavedChanges();
    // The active document saved to a path asked for: "Save As", and a save
    // of one that has no file yet.
    void SaveAs();
    void Save();
    // What the viewport shows: the active document's world - a scene's own,
    // a material's preview - and the camera on it; nothing for a document
    // that has no world, an animation graph's.
    struct ActiveView
    {
        World& mWorld;
        EditorCamera& mCamera;
        OptRef<DocumentWorld> mSceneMaybe;
    };
    opt<ActiveView> TryActiveView();
    // The active document's name as its tab shows it, scene or data.
    opt<string> TryActiveTitle();

    EditorProject& mProject;
    ScenePanel mScene;
    Inspector mInspector;
    AssetsPanel mAssets;
    Viewport mViewport;
    // The active document's tab as last shown: a document made active by an
    // operator gets its tab picked.
    string mActiveTab;
    string mSaveAsPath;
    bool mAskSaveAs = false;
    vector<LogEntry> mLog;
    u64 mLogNext = 0;
    f64 mSinceRefresh = 0;
    bool mLaidOut = false;
    bool mQuitAsked = false;
};
}
