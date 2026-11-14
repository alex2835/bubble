#pragma once
#include "bubble/editing/clipboard.hpp"
#include "bubble/editing/document.hpp"

namespace bubble
{
// What the editor has open: the documents, the one active - which the panels
// show and the operators change - and the clipboard they share. One for the
// editor; documents stay where they are while open.
class Workspace
{
public:
    explicit Workspace( const ComponentTypes& componentTypes ) : mComponentTypes( componentTypes ) {}
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
    size_t Count() const { return mDocuments.size(); }

    Clipboard& GetClipboard() { return mClipboard; }
    const Clipboard& GetClipboard() const { return mClipboard; }
    const ComponentTypes& GetComponentTypes() const { return mComponentTypes; }

private:
    vector<Scope<Document>>::iterator Find( const Document& document );

    const ComponentTypes& mComponentTypes;
    // The active one last: closing it goes back to the one before.
    vector<Scope<Document>> mDocuments;
    Clipboard mClipboard;
};
}
