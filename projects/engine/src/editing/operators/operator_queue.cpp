#include "engine/pch/pch.hpp"
#include "engine/editing/operators/operator_queue.hpp"
#include <nlohmann/json.hpp>

namespace bubble
{
/// OperatorQueue

void OperatorQueue::Enqueue( string name, json args )
{
    mPending.push_back( { std::move( name ), CreateScope<json>( std::move( args ) ) } );
}

void OperatorQueue::Enqueue( string name )
{
    Enqueue( std::move( name ), json::object() );
}

void OperatorQueue::Flush( OperatorContext& ctx )
{
    // Taken first: an operator may enqueue another, which then waits for
    // the next flush rather than running inside this one.
    vector<Call> calls = std::move( mPending );
    mPending.clear();
    for ( const auto& call : calls )
    {
        try
        {
            InvokeOperator( call.mName, ctx, *call.mArgs );
        }
        catch ( const std::exception& e )
        {
            LogError( "{}: {}", call.mName, e.what() );
        }
    }
}

}
