#include "bubble/scripts/script_runtime.hpp"

// Events between instances: on( event, fn ) and emit( event, ... ).
namespace bubble
{
void ScriptRuntime::EmitWith( string_view event, const LuaRest& args )
{
    const auto found = mEvents.find( event );
    if ( found == mEvents.end() )
        return;
    // A copy: subscribing or leaving during the calls changes the list,
    // not this pass over it.
    const vector<Subscriber> subscribers = found->second;
    for ( const Subscriber& subscriber : subscribers )
    {
        if ( not Enabled( subscriber.mInstanceHandle ) )
            continue;
        expected<LuaValue, ScriptError> called;
        {
            const CurrentScope scope( *this, subscriber.mInstanceHandle );
            called = subscriber.mFunction( Self( subscriber.mInstanceHandle ), args );
        }
        if ( not called )
            Fail( subscriber.mInstanceHandle, called.error() );
    }
}
}
