#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/function.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/pointer.hpp"
#include "bubble/types/thread.hpp"

namespace bubble
{
// The process's threads: one pool for everything that runs in parallel -
// physics, animation, the render's extract, importing - so nothing else
// knows enkiTS is under it. Only the main thread writes a scene. Work on the
// pool reads one inside a phase the main thread opens and waits for
// (ParallelFor), or touches none at all (Background).
//
// The web has no threads unless the page is served for them: there Jobs
// runs everything on the main thread - ParallelFor in place, background work
// when the main queue is run - and the code that uses it stays the same.
class Jobs
{
public:
    // Threads that do the work, the main thread among them; 0 is one a core.
    // The thread that makes Jobs is the main thread. With one, everything
    // runs on it, as on the web: a test that wants the order of things
    // fixed asks for that.
    explicit Jobs( u32 threads = 0 );
    // Waits for background work to finish. What waits for the main thread
    // then is dropped: there is no frame left to run it in.
    ~Jobs();
    Jobs( const Jobs& ) = delete;
    Jobs& operator=( const Jobs& ) = delete;

    u32 Threads() const;

    // fn( begin, end ) for ranges that cover [0, count), each at least
    // `grain` long but the last, on the pool and the calling thread, which
    // waits and helps. A range writes its own results - by index - so what
    // comes of it does not depend on which thread ran what. An exception from
    // fn is thrown here, once every range is done; with several, one of them.
    // The caller helps with nothing else meanwhile: the main thread does not
    // get stuck in an import while a phase waits.
    void ParallelFor( u32 count, u32 grain, const function<void( u32 begin, u32 end )>& fn );

    // Work that takes a while and touches no scene - an import, an asset
    // parsed - on the pool. What comes of it goes back through RunOnMain.
    // An exception from it is thrown on the main thread by RunMainQueue.
    // Called from another thread than the main one, a logic_error.
    void Background( function<void()> work );

    // From any thread: work for the main thread, run by RunMainQueue in the
    // order it was queued.
    void RunOnMain( function<void()> work );
    // On the main thread, once a frame: what RunOnMain queued. Anything
    // queued meanwhile waits for the next call. On another thread, a
    // logic_error.
    void RunMainQueue();

    // Where the work runs; Jobs' own business.
    struct Pool;

private:
    void RequireMain( string_view what ) const;

    thread::id mMain;
    mutex mQueueLock;
    vector<function<void()>> mQueue;
    // enkiTS, or the main thread alone. Gone first: background work it
    // waits for may still queue work for the main thread.
    Scope<Pool> mPool;
};
}
