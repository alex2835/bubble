#pragma once
#include "bubble/editor/editor_project.hpp"
#include <imgui.h>

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

// The icon a file of the project is shown by: its kind's - a level, a
// prefab, a material, a model, a sound, a script... -, a plain file's for a
// kind it does not know.
string_view FileIcon( string_view path, string_view kind );

// How the assets panel shows the files of a folder: tiles - a picture or an
// icon each, the name under it -, or a list of rows with their kinds.
enum class AssetsView : u8
{
    Tiles,
    List,
};

// The panel of the project's files: its folders, the files of the one
// picked - as tiles or rows, their kinds, a file that would not import
// marked with why -, a file dragged out to a field, the tree or the
// viewport. A level or a
// prefab opens as a document at a double click; anything else as the
// system opens it. New assets from templates, new folders, renaming in place
// and deleting through the asset operators.
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
    AssetsView View() const { return mView; }
    void SetView( AssetsView view ) { mView = view; }
    // A tile's side, in ImGui's units.
    f32 TileSize() const { return mTileSize; }
    void SetTileSize( f32 size );
    // Where a file's tile or row was drawn last - a folder's, its path and
    // "/" -, its middle in ImGui's units: for tests.
    opt<vec2> TryWhere( string_view path ) const;
    // The file a click picked since the last ask: what the inspector shows.
    opt<string> TakePicked();
    // What shows a texture's file: its picture as ImGui takes one and its
    // size in pixels, nothing while it loads - or for no texture.
    struct Picture
    {
        ImTextureID mId = 0;
        vec2 mSize{ 1.0f };
    };
    using Thumbnail = function<opt<Picture>( const AssetPath& path )>;
    void SetThumbnail( Thumbnail thumbnail ) { mThumbnail = std::move( thumbnail ); }
    // A folder named `name` in the one shown, shown from now on: the
    // panel's until a file goes in. A name with a slash, or starting with a
    // dot, makes none.
    bool MakeFolder( string_view name );

private:
    void List();
    void FolderRow( const string& folder );
    void Files();
    void Header();
    void Tiles();
    void FolderTile( const string& folder );
    void FileTile( const string& path );
    void FileRow( const string& path );
    // What a file's tile and row do alike: picked, opened, dragged, its menu,
    // its error told.
    void FileActions( const string& path, OptRef<const string> errorMaybe );
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
    Thumbnail mThumbnail;
    // Folders made here, kept until a file goes in and the disk has them.
    vector<string> mMadeFolders;
    string mNewFolder;
    hmap<string, vec2> mWhere;
    AssetsView mView = AssetsView::Tiles;
    f32 mTileSize = 84.0f;
};
}
