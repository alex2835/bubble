#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/function.hpp"
#include "bubble/types/handle.hpp"
#include "bubble/types/utility.hpp"
#include <entt/core/any.hpp>
#include <entt/core/type_info.hpp>

namespace bubble
{
using EventHandle = Handle<struct EventTag>;

// What happened in a world - a collision, a door that opened - from the
// module that saw it to whoever listens, by the event's type:
//
//     events.Subscribe<Collision>( [this]( const Collision& hit ) { ... } );
//     events.Emit( Collision{ a, b, point } );
//
// Emit only queues. The world delivers at its own places - after each
// step's physics, and before the frame's own phases what came since -, so a
// listener runs where the frame order says, never in the middle of whoever
// emitted.
class Events
{
public:
    template <typename E>
    EventHandle Subscribe( function<void( const E& )> fn )
    {
        const u32 type = entt::type_hash<E>::value();
        const EventHandle handle = mSubscribers.Add( Subscriber{
            type, [fn = std::move( fn )]( const entt::any& event ) { fn( entt::any_cast<const E&>( event ) ); } } );
        mByType[type].push_back( handle );
        return handle;
    }

    // A stale handle is left alone. A listener may take itself or another
    // off while it runs.
    void Unsubscribe( EventHandle handle );

    template <typename E>
    void Emit( E event )
    {
        static_assert( copy_constructible<E>, "an event is copied by a world's snapshot" );
        mPending.push_back( Emitted{ entt::type_hash<E>::value(), entt::any( std::move( event ) ) } );
    }

    // Calls the listeners of what is waiting: the events in the order they
    // were emitted, and each one's listeners in the order they subscribed.
    // One emitted meanwhile is delivered in this same call, after those
    // waiting; one subscribed meanwhile hears the next event, not this one.
    // Delivering from inside a listener is a logic_error.
    void Deliver();

    size_t Waiting() const { return mPending.size(); }

    // One event waiting, its type erased.
    struct Emitted
    {
        u32 mType = 0;
        entt::any mEvent;
    };

    // What waits, copied, and put back in place of what waits then: a
    // world's snapshot delivers what the world it was taken from would.
    // Either from inside a listener is a logic_error.
    vector<Emitted> CopyWaiting() const;
    void RestoreWaiting( vector<Emitted> waiting );

private:
    struct Subscriber
    {
        u32 mType = 0;
        function<void( const entt::any& )> mFn;
    };

    SlotMap<Subscriber, EventTag> mSubscribers;
    // By the event's type, in the order they subscribed. Handles taken off
    // during a delivery stay until it ends, stale.
    hmap<u32, vector<EventHandle>> mByType;
    vector<Emitted> mPending;
    bool mDelivering = false;
};
}
