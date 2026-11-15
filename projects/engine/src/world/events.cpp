#include "bubble/world/events.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
void Events::Unsubscribe( EventHandle handle )
{
    const auto subscriberMaybe = mSubscribers.TryGet( handle );
    if ( not subscriberMaybe )
        return;
    const u32 type = subscriberMaybe->mType;
    mSubscribers.Remove( handle );
    // A delivery walks the list by position: it is tidied once it ends.
    if ( not mDelivering )
        erase( mByType[type], handle );
}

void Events::Deliver()
{
    if ( mDelivering )
        throw logic_error( "Events::Deliver from inside a listener" );
    mDelivering = true;
    for ( size_t at = 0; at < mPending.size(); ++at )
    {
        // Moved out: a listener that emits grows the queue under it.
        const Emitted emitted = std::move( mPending[at] );
        const auto found = mByType.find( emitted.mType );
        if ( found == mByType.end() )
            continue;
        // The map's nodes stay put while it grows, so the list does too.
        const vector<EventHandle>& handles = found->second;
        const size_t count = handles.size();
        for ( size_t i = 0; i < count; ++i )
        {
            const auto subscriberMaybe = mSubscribers.TryGet( handles[i] );
            if ( not subscriberMaybe )
                continue;
            // A copy: the listener may take itself off while it runs.
            const function<void( const entt::any& )> fn = subscriberMaybe->mFn;
            fn( emitted.mEvent );
        }
    }
    mPending.clear();
    for ( auto& [type, handles] : mByType )
        erase_if( handles, [this]( EventHandle handle ) { return not mSubscribers.Alive( handle ); } );
    mDelivering = false;
}
}
