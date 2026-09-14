#include "engine/pch/pch.hpp"
#include "engine/editing/lua_value_command.hpp"
#include <sol/sol.hpp>

namespace bubble
{
string LuaPathToString( const LuaPath& path )
{
    string result;
    for ( const auto& key : path )
    {
        std::visit( [&]( const auto& k )
        {
            if constexpr ( std::is_same_v<std::decay_t<decltype( k )>, int> )
                result += std::format( "[{}]", k );
            else
                result += ( result.empty() ? "" : "." ) + k;
        }, key );
    }
    return result;
}

opt<Table> LuaTableRoot::Get() const
{
    if ( not mScene or not mGet or not mScene->HasEntity( mEntity ) )
        return std::nullopt;
    return mGet( *mScene, mEntity );
}

namespace
{
template <typename F>
auto WithKey( const LuaKey& key, F&& f )
{
    return std::visit( std::forward<F>( f ), key );
}
}

opt<Table> LuaParentTable( const LuaTableRoot& root, const LuaPath& path )
{
    auto table = root.Get();
    if ( not table or path.empty() )
        return std::nullopt;

    for ( size_t i = 0; i + 1 < path.size(); i++ )
    {
        sol::object next = WithKey( path[i], [&]( const auto& k ) { return ( *table )[k].template get<sol::object>(); } );
        if ( not next.is<Table>() )
            return std::nullopt;
        table = next.as<Table>();
    }
    return table;
}

void SetLuaValue( const LuaTableRoot& root, const LuaPath& path, const Any& value )
{
    auto parent = LuaParentTable( root, path );
    if ( not parent )
        return;
    const Any stored = value.is<Table>() ? AnyDeepCopy( value ) : value;
    WithKey( path.back(), [&]( const auto& k ) { ( *parent )[k] = stored; } );
}

SetLuaValueCommand::SetLuaValueCommand( LuaTableRoot root, LuaPath path, const Any& oldValue, const Any& newValue )
    : mRoot( std::move( root ) ),
      mPath( std::move( path ) ),
      mOld( CreateScope<Any>( AnyDeepCopy( oldValue ) ) ),
      mNew( CreateScope<Any>( AnyDeepCopy( newValue ) ) ),
      mName( std::format( "{}.{}", mRoot.mName, LuaPathToString( mPath ) ) )
{
}

SetLuaValueCommand::~SetLuaValueCommand() = default;

void SetLuaValueCommand::Execute() { SetLuaValue( mRoot, mPath, *mNew ); }
void SetLuaValueCommand::Undo() { SetLuaValue( mRoot, mPath, *mOld ); }

}
