#pragma once
#include "bubble/data/json.hpp"
#include "bubble/editing/document.hpp"

namespace bubble
{
// What was copied, for a paste into any document: entities with what is
// under them as a file holds them, so a copy outlives the document it came
// from and goes into another - from a prefab into a level. One for the
// editor, not one per document.
class Clipboard
{
public:
    struct CopiedEntity
    {
        Json mEntity;
        // Where it was: absolute links into it lead into a pasted copy.
        EntityPath mFrom;
    };

    // `entities` with what is under them, in place of what was here. Give
    // the top of a selection: one under another would come twice. The root
    // is not copied - it is the scene. On an error what was here stays.
    expected<void, string> TryCopy( const Document& document, span<const Entity> entities );

    bool Empty() const { return mCopied.empty(); }
    span<const CopiedEntity> All() const { return mCopied; }

private:
    vector<CopiedEntity> mCopied;
};
}
