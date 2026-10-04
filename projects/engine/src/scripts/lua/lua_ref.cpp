#include "bubble/scripts/lua/lua_ref.hpp"
#include <lua.h>
#include <utility>

namespace bubble
{
LuaRef::LuaRef( lua_State* L, int index )
    : mL( lua_mainthread( L ) ),
      mRef( lua_ref( L, index ) )
{
}

LuaRef::~LuaRef()
{
    Reset();
}

LuaRef::LuaRef( LuaRef&& other ) noexcept
    : mL( std::exchange( other.mL, nullptr ) ),
      mRef( std::exchange( other.mRef, LUA_NOREF ) )
{
}

LuaRef& LuaRef::operator=( LuaRef&& other ) noexcept
{
    if ( this != &other )
    {
        Reset();
        mL = std::exchange( other.mL, nullptr );
        mRef = std::exchange( other.mRef, LUA_NOREF );
    }
    return *this;
}

LuaRef LuaRef::Copy() const
{
    if ( Empty() )
        return {};
    Push( mL );
    LuaRef copy( mL, -1 );
    lua_pop( mL, 1 );
    return copy;
}

void LuaRef::Reset()
{
    if ( not Empty() )
        lua_unref( mL, mRef );
    mRef = LUA_NOREF;
}

void LuaRef::Push( lua_State* L ) const
{
    if ( Empty() )
        lua_pushnil( L );
    else
        lua_getref( L, mRef );
}
}
