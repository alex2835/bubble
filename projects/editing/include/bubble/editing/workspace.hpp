#pragma once
#include "bubble/editing/clipboard.hpp"
#include "bubble/editing/document.hpp"
#include "bubble/editing/project_files.hpp"

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
};
}
