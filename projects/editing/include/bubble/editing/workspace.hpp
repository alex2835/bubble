#pragma once
#include "bubble/editing/clipboard.hpp"
#include "bubble/editing/data_document.hpp"
#include "bubble/editing/document.hpp"
#include "bubble/import/project_files.hpp"
#include "bubble/scene/prefab.hpp"

namespace bubble
{
// What the editor has open: the documents - levels and prefabs as scenes,
// materials and animation graphs as data -, the one active, of either kind,
// which the panels show and the operators change, and the clipboard they
// share. One for the editor; documents stay where they are while open.
//
// The project's catalog is the scan's; the workspace adds to it the levels
// and prefabs it writes.
class Workspace
{
public:
    Workspace( const ComponentTypes& componentTypes, ProjectFiles files, AssetCatalog& assets )
        : mComponentTypes( componentTypes ),
          mFiles( std::move( files ) ),
          mAssets( assets )
    {
    }
    Workspace( const Workspace& ) = delete;
    Workspace& operator=( const Workspace& ) = delete;

    // A document of `scene`, active from now on.
    Document& Open( Scene scene = Scene() );
    // The one active before it is active again. A document not open here is
    // a logic_error, as is closing one while an operator works on it.
    void Close( Document& document );
    void Activate( Document& document );

    // The scene document active - nothing while a data document is.
    OptRef<Document> TryActive();
    OptRef<const Document> TryActive() const;
    // The open document saved to `path`.
    OptRef<Document> TryFind( const AssetPath& path );

    // A data document of the file at `path`, active from now on.
    DataDocument& OpenData( AssetPath path, Json data );
    void Close( DataDocument& document );
    void Activate( DataDocument& document );
    // The data document active - nothing while a scene document is.
    OptRef<DataDocument> TryActiveData();
    OptRef<const DataDocument> TryActiveData() const;
    OptRef<DataDocument> TryFindData( const AssetPath& path );
    // The active one last.
    span<const Scope<DataDocument>> AllData() const { return mData; }
    size_t Count() const { return mDocuments.size(); }
    // The active one last.
    span<const Scope<Document>> All() const { return mDocuments; }

    // The id of the level or prefab at `path`, as the catalog has it. One
    // the catalog does not have yet - a file made since the scan - is taken
    // in now, under the id the file keeps inside if no other asset has it,
    // or a new one. A file that is not there is an error.
    expected<AssetId, string> TryIdOf( const AssetPath& path );
    // The same for a file about to be written: one not there yet gets a new
    // id.
    AssetId IdOf( const AssetPath& path );

    // The prefab with the id, read once and kept until a save here changes
    // it.
    expected<Ref<const Prefab>, string> TryPrefab( const AssetId& prefabId );
    // A file moved or deleted by an operator: the catalog follows at once -
    // the scan would only at the next look -, and so does the path of a
    // document open on it.
    void Moved( const AssetPath& from, const AssetPath& to );
    void Removed( const AssetPath& path );
    // The prefab is `prefab` from now on: what a save here wrote.
    void SetPrefab( const AssetId& prefabId, Ref<const Prefab> prefab ) { mPrefabs[prefabId] = std::move( prefab ); }
    // What levels and prefabs are read and written with: the catalog, and
    // the prefabs of TryPrefab.
    FileContext GetFileContext();

    const ProjectFiles& GetFiles() const { return mFiles; }
    const AssetCatalog& GetAssets() const { return mAssets; }
    Clipboard& GetClipboard() { return mClipboard; }
    const Clipboard& GetClipboard() const { return mClipboard; }
    const ComponentTypes& GetComponentTypes() const { return mComponentTypes; }

private:
    vector<Scope<Document>>::iterator Find( const Document& document );
    AssetId Take( const AssetPath& path, string_view text );

    const ComponentTypes& mComponentTypes;
    ProjectFiles mFiles;
    AssetCatalog& mAssets;
    // The active one last: closing it goes back to the one before.
    vector<Scope<Document>> mDocuments;
    vector<Scope<DataDocument>> mData;
    // Which kind the active document is: the last of its list.
    bool mDataActive = false;
    Clipboard mClipboard;
    hmap<AssetId, Ref<const Prefab>> mPrefabs;
};
}
