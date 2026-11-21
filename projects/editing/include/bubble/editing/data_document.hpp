#pragma once
#include "bubble/core/asset_path.hpp"
#include "bubble/data/json.hpp"
#include "bubble/editing/document.hpp"
#include "bubble/reflection/field_path.hpp"

namespace bubble
{
// A file of data open in the editor - a material, an animation graph - as
// its JSON, with a history of its own: undo in a material does not take back
// the level. What it holds changes through operators alone (data.set,
// data.remove), each change a step; in a group - a drag in the inspector -
// a value set again at the same path folds into the first set.
//
// Paths go through objects by their members' names and through arrays by
// positions from 1: "params.roughness", "transitions[2].when".
class DataDocument
{
public:
    DataDocument( AssetPath path, Json data );
    DataDocument( const DataDocument& ) = delete;
    DataDocument& operator=( const DataDocument& ) = delete;

    const Json& GetData() const { return mData; }
    const AssetPath& GetPath() const { return mPath; }
    void SetPath( AssetPath path ) { mPath = std::move( path ); }
    // Moves on with every change, undone and redone ones too: what a view
    // built from the data - a material's preview - compares to know it is
    // stale.
    u64 Version() const { return mVersion; }

    // `value` at `path`: an object's member is made if it is not there; a
    // position in an array has to be there, or be one past the end.
    expected<void, string> TrySet( const FieldPath& path, Json value, string label );
    // The member or the element at `path` taken out.
    expected<void, string> TryRemove( const FieldPath& path, string label );
    // What is at `path`; nothing when it is not there.
    OptRef<const Json> TryAt( const FieldPath& path ) const;

    void Undo();
    void Redo();
    bool CanUndo() const { return not mUndo.empty(); }
    bool CanRedo() const { return not mRedo.empty(); }
    // The changes until EndGroup are one step, `label`; groups nest, the
    // outer one's label standing. CancelGroup takes back what the group
    // begun last did.
    void BeginGroup( string label );
    void EndGroup();
    void CancelGroup();
    bool InGroup() const { return not mGroupStarts.empty(); }

    // Whether the data is not what was last saved: by the history's place,
    // so undo back to the save and it is clean.
    bool IsDirty() const { return Position() != mSavedAt; }
    void MarkSaved() { mSavedAt = Position(); }

    // The editor's view of it - a material's preview -: nothing until the
    // editor shows it.
    OptRef<DocumentView> TryView();
    void SetView( Scope<DocumentView> view ) { mView = std::move( view ); }

private:
    // One value changed: what was at the path and what is, nothing for
    // absent.
    struct Change
    {
        FieldPath mPath;
        opt<Json> mBefore;
        opt<Json> mAfter;
    };

    struct Step
    {
        u64 mId = 0;
        string mLabel;
        vector<Change> mChanges;
    };

    expected<void, string> TryChange( const FieldPath& path, opt<Json> value, string label );
    // `value` put at `path` - or what is there taken out, for nothing -
    // with no history.
    expected<void, string> TryPut( const FieldPath& path, const opt<Json>& value );
    void Push( Step step );
    u64 Position() const { return mUndo.empty() ? 0 : mUndo.back().mId; }

    AssetPath mPath;
    Json mData;
    vector<Step> mUndo;
    vector<Step> mRedo;
    // The group's changes as they come; how many it had as each nested
    // group began.
    Step mGroup;
    vector<size_t> mGroupStarts;
    u64 mNextId = 1;
    u64 mSavedAt = 0;
    u64 mVersion = 0;
    // Last, so it goes first.
    Scope<DocumentView> mView;
};
}
