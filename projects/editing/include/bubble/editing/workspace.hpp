#pragma once
#include "bubble/editing/clipboard.hpp"
#include "bubble/editing/document.hpp"
#include "bubble/import/project_files.hpp"
#include "bubble/scene/prefab.hpp"

namespace bubble
{
// What the editor has open: the documents, the one active - which the panels
// show and the operators change - and the clipboard they share. One for the
// editor; documents stay where they are while open.
class Workspace
{
public:
    Workspace( const ComponentTypes& componentTypes, ProjectFiles files )
        : mComponentTypes( componentTypes ),
          mFiles( std::move( files ) )
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

    OptRef<Document> TryActive();
    OptRef<const Document> TryActive() const;
    // The open document saved to `path`.
    OptRef<Document> TryFind( const AssetPath& path );
    size_t Count() const { return mDocuments.size(); }
    // The active one last.
    span<const Scope<Document>> All() const { return mDocuments; }

    // The prefab at `path`, read once and kept until a save here changes it.
    expected<Ref<const Prefab>, string> TryPrefab( const AssetPath& path );
    // What levels are read and written with: TryPrefab.
    PrefabSource Prefabs();
    // The prefab at `path` is `prefab` from now on: what a save here wrote.
    void SetPrefab( const AssetPath& path, Ref<const Prefab> prefab ) { mPrefabs[path] = std::move( prefab ); }

    const ProjectFiles& GetFiles() const { return mFiles; }
    Clipboard& GetClipboard() { return mClipboard; }
    const Clipboard& GetClipboard() const { return mClipboard; }
    const ComponentTypes& GetComponentTypes() const { return mComponentTypes; }

private:
    vector<Scope<Document>>::iterator Find( const Document& document );

    const ComponentTypes& mComponentTypes;
    ProjectFiles mFiles;
    // The active one last: closing it goes back to the one before.
    vector<Scope<Document>> mDocuments;
    Clipboard mClipboard;
    hmap<AssetPath, Ref<const Prefab>> mPrefabs;
};
}
