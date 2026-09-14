#include "engine/pch/pch.hpp"
#include "engine/editing/operators/operator.hpp"
#include <nlohmann/json.hpp>

namespace bubble
{
OperatorRegistry& OperatorRegistry::Instance()
{
    static OperatorRegistry registry;
    return registry;
}

void OperatorRegistry::Register( Operator op )
{
    if ( Find( op.mName ) )
        throw std::runtime_error( std::format( "Operator registered twice: {}", op.mName ) );
    if ( not op.mExec )
        throw std::runtime_error( std::format( "Operator without an exec: {}", op.mName ) );
    mOperators.push_back( std::move( op ) );
}

const Operator* OperatorRegistry::Find( string_view name ) const
{
    const auto it = std::ranges::find( mOperators, name, &Operator::mName );
    return it == mOperators.end() ? nullptr : &*it;
}

vector<string_view> OperatorRegistry::Names() const
{
    vector<string_view> names;
    names.reserve( mOperators.size() );
    for ( const auto& op : mOperators )
        names.push_back( op.mName );
    return names;
}

namespace
{
const Operator& Require( string_view name )
{
    const Operator* op = OperatorRegistry::Instance().Find( name );
    if ( not op )
        throw std::runtime_error( std::format( "No such operator: {}", name ) );
    return *op;
}
}

bool PollOperator( string_view name, const OperatorContext& ctx, const json& args )
{
    const Operator& op = Require( name );
    return not op.mPoll or op.mPoll( ctx, args );
}

bool PollOperator( string_view name, const OperatorContext& ctx )
{
    return PollOperator( name, ctx, json::object() );
}

bool InvokeOperator( string_view name, OperatorContext& ctx, const json& args )
{
    const Operator& op = Require( name );
    if ( op.mPoll and not op.mPoll( ctx, args ) )
        return false;
    op.mExec( ctx, args );
    return true;
}

bool InvokeOperator( string_view name, OperatorContext& ctx )
{
    return InvokeOperator( name, ctx, json::object() );
}

}
