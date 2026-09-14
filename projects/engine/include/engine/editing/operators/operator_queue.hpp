#pragma once
#include "engine/editing/operators/operator.hpp"
#include "engine/editing/operators/operator_queue.hpp"
#include "engine/types/pointer.hpp"

namespace bubble
{
// Operators to run later, at a point in the frame where nothing is in the
// middle of drawing the document they change: opening a level from a menu
// item pulls the level out from under every other window still drawing it,
// so the menu enqueues and the editor flushes before the next frame's UI.
class OperatorQueue
{
public:
    void Enqueue( string name, json args );
    void Enqueue( string name );

    // Runs everything queued, in order, each one logged if it throws so one
    // bad call does not drop the ones behind it.
    void Flush( OperatorContext& ctx );

    bool Empty() const { return mPending.empty(); }

private:
    struct Call
    {
        string mName;
        Scope<json> mArgs; // json is only forward-declared here
    };
    vector<Call> mPending;
};

}
