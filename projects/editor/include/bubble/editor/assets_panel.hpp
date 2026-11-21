#pragma once
#include "bubble/editor/editor_project.hpp"

namespace bubble
{
// What the assets panel's files carry when dragged: the file's path, as
// text. A field of the asset's kind takes one; the tree and the viewport
// place a prefab or a model dropped on them.
constexpr const char* cAssetPayload = "bubble.asset";

// The asset at `path` put into the active document under `parent`: a
// prefab instantiated, a model as an entity of its own with it - at
// `position` when one is given -, selected, as one step. Anything else, or
// a failure on the way, leaves the document as it was and says why.
expected<void, string> TryPlaceAsset( EditorProject& project,
                                      const AssetPath& path,
                                      const EntityPath& parent,
                                      opt<vec3> position = nullopt );

// The panel of the project's files: its folders, the files of the one
// picked - their kinds, a file that would not import marked with why -, a
// file dragged out to a field, the tree or the viewport. A level or a
// prefab opens as a document at a double click; anything else as the
// system opens it. New assets from templates, renaming in place and
// deleting through the asset operators.
class AssetsPanel
{
public:
    // `open` hands a file to the system: a script to the editor people use.
    AssetsPanel( EditorProject& project, function<void( const OsPath& )> open );

    // Its widgets, inside an ImGui window.
    void Show();
    // The folder whose files are shown, "" for the project's own.
    const string& Folder() const { return mFolder; }
    void SetFolder( string folder ) { mFolder = std::move( folder ); }
    // Where a file's row was drawn last, its middle in ImGui's units: for
    // tests.
    opt<vec2> TryWhere( string_view path ) const;
    // The file a click picked since the last ask: what the inspector shows.
    opt<string> TakePicked();

private:
    void List();
    void FolderRow( const string& folder );
    void Files();
    void FileRow( const string& path );
    void FileMenu( const string& path );
    void BackgroundMenu();
    void Rename( const string& path );
    void DeleteDialog();
    void New( string_view stem, string_view extension );
    void Open( const string& path );
    bool Run( string_view op, const Json& args );

    EditorProject& mProject;
    function<void( const OsPath& )> mOpen;
    // The project's files - no .import - and every folder with something
    // in it, as last listed.
    vector<string> mFiles;
    vector<string> mFolders;
    f64 mListedAt = -1;
    string mFolder;
    string mSelected;
    opt<string> mPickedMaybe;
    string mRenaming;
    string mNewName;
    bool mRenameStarted = false;
    string mDeleting;
    hmap<string, vec2> mWhere;
};
}
