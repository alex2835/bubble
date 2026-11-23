#pragma once
#include "bubble/assets/texture_asset.hpp"
#include "bubble/core/log.hpp"
#include "bubble/editor/assets_panel.hpp"
#include "bubble/editor/document_world.hpp"
#include "bubble/editor/editor_project.hpp"
#include "bubble/editor/inspector.hpp"
#include "bubble/editor/preferences_panel.hpp"
#include "bubble/editor/project_settings_panel.hpp"
#include "bubble/editor/scene_panel.hpp"
#include "bubble/editor/thumbnail_service.hpp"
#include "bubble/editor/viewport.hpp"
#include "bubble/editor/world_picture.hpp"
#include "bubble/input/input_event.hpp"

namespace bubble
{
// The editor's window as ImGui draws it, over a project: the dock space, the
// menu, the panels - the scene's tree, the viewport with a tab a level or a
// prefab, the inspector, the project's assets, the log -, and a window of
// its own for each material and animation graph open, docked beside the
// viewport the first time. The tree and the viewport show the scene active
// last; the inspector, undo and save the active document - the one turned
// to last, by a click in its window or a tab picked. Operators alone change
// anything.
//
// Play (Ctrl+P) plays the scene shown on a copy of it, a tab of its own,
// shown through the game's camera; the game has the mouse and the keys from
// then on - Shift+F1 or a click elsewhere takes them back, a click on it
// gives them again -, and Stop throws the copy away.
class Editor
{
public:
    // The project, the preferences, the Gpu and the renderer outlive it;
    // `open` hands a file to the system, as the platform does.
    Editor( EditorProject& project,
            Preferences& preferences,
            Gpu& gpu,
            Renderer& renderer,
            function<void( const OsPath& )> open );
    Editor( const Editor& ) = delete;
    Editor& operator=( const Editor& ) = delete;

    // How the editor looks, as the preferences say: its font - Roboto built
    // in, or a file -, glyphs made as text needs them, Cyrillic too, the
    // icons merged in; its style at the display's `displayScale` times the
    // preferences' own; docking. Before the context's first frame, and again
    // between frames when the appearance changes.
    static void SetUpImGui( const AppearancePreferences& appearance, f32 displayScale );

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
    // The player's input as the platform polled it, beside ImGui's: the
    // game's while it has the viewport; the mouse's moves the viewport's
    // camera's too.
    void Handle( const InputEvent& event );
    // Whether the cursor is to be held - a camera turned by the mouse -: the
    // viewport flying or going round, or the game wanting it with the input
    // to hold it with.
    bool CursorLocked();
    // To quit - Quit picked, the window closed -: at once when nothing is
    // left not saved, after asking when something is.
    void AskToQuit() { mQuitWanted = true; }
    // To close the project and go back to the start window, asking first
    // as quitting does.
    void AskToClose() { mCloseWanted = true; }
    // Where the MCP server listens, shown on the menu's bar: agents drive
    // the editor there.
    // Empty: no server.
    void SetMcpUrl( string url ) { mMcpUrl = std::move( url ); }
    // Quit for good: nothing not saved, or the user said so.
    bool QuitAsked() const { return mQuitAsked; }
    // Close the project: nothing not saved, or the user said so.
    bool CloseAsked() const { return mCloseAsked; }
    Viewport& GetViewport() { return mViewport; }

private:
    void Layout( u32 dockSpace );
    void Menu();
    void MenuItem( const char* label, const char* keys, string_view op );
    // Play, Pause, Step, Stop: buttons on the menu's bar.
    void PlayButtons();
    // Whether the game has the input from now on: playing, shown, not taken
    // back.
    void GameInput();
    void Shortcuts();
    void LogPanel();
    void DocumentTabs();
    // A window a data document: a graph's canvas, a material's preview.
    void AssetWindows();
    // Before the dock space: a data document's window shown for the first
    // time - or whose place went with the last window in it - docked beside
    // the viewport, with the other ones.
    void DockAssetWindows();
    void UnsavedChanges();
    // Quitting with documents not saved: save them all, lose them, or stay.
    void QuitDialog();
    // The active document saved to a path asked for: "Save As", and a save
    // of one that has no file yet.
    void SaveAs();
    void Save();
    // The world of the scene shown - the one active last -, and the camera
    // on it.
    OptRef<DocumentWorld> TryShownWorld();
    // The active document's name as its tab or window shows it.
    opt<string> TryActiveTitle();

    EditorProject& mProject;
    Preferences& mPreferences;
    Gpu& mGpu;
    Renderer& mRenderer;
    ScenePanel mScene;
    Inspector mInspector;
    AssetsPanel mAssets;
    Viewport mViewport;
    PreferencesPanel mPreferencesPanel;
    ProjectSettingsPanel mProjectSettings;
    string mMcpUrl;
    // The shown scene's tab as last drawn: a scene made active by an
    // operator gets its tab picked.
    string mActiveTab;
    // The active document as of the last frame: a data document made active
    // by an operator gets its window brought forward.
    const void* mLastActive = nullptr;
    // Each material window's picture, by its document; the ones shown this
    // frame, drawn after the widgets.
    hmap<const DataDocument*, Scope<WorldPicture>> mPictures;
    vector<DataDocument*> mPictured;
    // Data documents' windows not shown this frame: waiting for the viewport
    // to be docked beside.
    hset<string> mHeld;
    // The textures the assets panel shows, held while the editor is.
    hmap<string, AssetRef<TextureAsset>> mThumbnails;
    // The pictures of models and materials, and the assets panel's handles
    // to them by path.
    ThumbnailService mThumbnailService;
    hmap<string, ThumbnailHandle> mThumbnailHandles;
    string mSaveAsPath;
    bool mAskSaveAs = false;
    bool mAskUnsaved = false;
    // The game has the input; Shift+F1 took it back until the next click.
    bool mGameInput = false;
    bool mReleased = false;
    bool mWasPlaying = false;
    vector<LogEntry> mLog;
    u64 mLogNext = 0;
    f64 mSinceRefresh = 0;
    bool mLaidOut = false;
    bool mQuitAsked = false;
    bool mQuitWanted = false;
    bool mCloseAsked = false;
    bool mCloseWanted = false;
    // What the question about what is not saved leads to: quitting, or
    // closing the project.
    bool mClosing = false;
};
}
