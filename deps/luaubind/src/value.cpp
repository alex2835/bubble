#include "luaubind/value.hpp"
#include "luaubind/state.hpp"
#include <lua.h>
#include <stdexcept>
#include <utility>

namespace luaubind
{
namespace
{
LuaKind KindOf( int type )
{
    switch ( type )
    {
        case LUA_TNIL: return LuaKind::Nil;
        case LUA_TBOOLEAN: return LuaKind::Boolean;
        case LUA_TNUMBER: return LuaKind::Number;
        case LUA_TSTRING: return LuaKind::String;
        case LUA_TVECTOR: return LuaKind::Vector;
        case LUA_TTABLE: return LuaKind::Table;
        case LUA_TFUNCTION: return LuaKind::Function;
        case LUA_TUSERDATA:
        case LUA_TLIGHTUSERDATA: return LuaKind::Userdata;
        case LUA_TTHREAD: return LuaKind::Thread;
        default: return LuaKind::Other;
    }
}

// The value if it is of `kind`, else nil: what the typed wrappers keep.
LuaValue OnlyIf( LuaValue value, LuaKind kind )
{
    return value.Is( kind ) ? std::move( value ) : LuaValue();
}
}

// ---- LuaValue --------------------------------------------------------------

LuaValue::LuaValue( lua_State* L, int index ) : mL( lua_mainthread( L ) ), mRef( lua_ref( L, index ) )
{
}

LuaValue::~LuaValue()
{
    Release();
}

LuaValue::LuaValue( const LuaValue& other ) : mL( other.mL )
{
    if ( other.IsNil() )
        return;
    lua_State* L = LuaState::Of( mL ).Active();
    other.Push( L );
    mRef = lua_ref( L, -1 );
    lua_pop( L, 1 );
}

LuaValue& LuaValue::operator=( const LuaValue& other )
{
    if ( this != &other )
        *this = LuaValue( other );
    return *this;
}

LuaValue::LuaValue( LuaValue&& other ) noexcept
    : mL( std::exchange( other.mL, nullptr ) ),
      mRef( std::exchange( other.mRef, LUA_NOREF ) )
{
}

LuaValue& LuaValue::operator=( LuaValue&& other ) noexcept
{
    if ( this != &other )
    {
        Release();
        mL = std::exchange( other.mL, nullptr );
        mRef = std::exchange( other.mRef, LUA_NOREF );
    }
    return *this;
}

void LuaValue::Release()
{
    if ( not IsNil() )
        lua_unref( mL, mRef );
    mRef = LUA_NOREF;
}

LuaKind LuaValue::Kind() const
{
    if ( IsNil() )
        return LuaKind::Nil;
    lua_State* L = LuaState::Of( mL ).Active();
    Push( L );
    const LuaKind kind = KindOf( lua_type( L, -1 ) );
    lua_pop( L, 1 );
    return kind;
}

bool LuaValue::Truthy() const
{
    if ( IsNil() )
        return false;
    lua_State* L = LuaState::Of( mL ).Active();
    Push( L );
    const bool truthy = lua_toboolean( L, -1 );
    lua_pop( L, 1 );
    return truthy;
}

string LuaValue::Describe() const
{
    if ( IsNil() )
        return "nil";
    return LuaState::Of( mL ).Describe( *this );
}

bool operator==( const LuaValue& a, const LuaValue& b )
{
    if ( a.IsNil() or b.IsNil() )
        return a.IsNil() == b.IsNil();
    lua_State* L = LuaState::Of( a.mL ).Active();
    a.Push( L );
    b.Push( L );
    const bool same = lua_rawequal( L, -1, -2 );
    lua_pop( L, 2 );
    return same;
}

void LuaValue::Push( lua_State* L ) const
{
    if ( IsNil() )
        lua_pushnil( L );
    else
        lua_getref( L, mRef );
}

LuaState* LuaValue::State() const
{
    return IsNil() ? nullptr : &LuaState::Of( mL );
}

// ---- LuaTable --------------------------------------------------------------

LuaTable::LuaTable( LuaValue value ) : LuaValue( OnlyIf( std::move( value ), LuaKind::Table ) )
{
}

vector<std::pair<LuaValue, LuaValue>> LuaTable::Pairs() const
{
    vector<std::pair<LuaValue, LuaValue>> pairs;
    if ( IsNil() )
        return pairs;
    State()->ForEach( *this, [&]( const LuaValue& key, const LuaValue& value ) { pairs.emplace_back( key, value ); } );
    return pairs;
}

int LuaTable::Length() const
{
    return IsNil() ? 0 : State()->Length( *this );
}

void LuaTable::SetMetatable( const LuaTable& metatable ) const
{
    if ( IsNil() )
        throw std::logic_error( "SetMetatable on nil, not a table" );
    State()->SetMetatable( *this, metatable );
}

void LuaTable::Freeze() const
{
    if ( not IsNil() )
        State()->Freeze( *this );
}

bool LuaTable::Frozen() const
{
    return not IsNil() and State()->Frozen( *this );
}

LuaTable LuaTable::Clone() const
{
    return IsNil() ? LuaTable() : State()->Clone( *this );
}

// ---- LuaFunction, LuaThread ------------------------------------------------

LuaFunction::LuaFunction( LuaValue value ) : LuaValue( OnlyIf( std::move( value ), LuaKind::Function ) )
{
}

LuaThread::LuaThread( LuaValue value ) : LuaValue( OnlyIf( std::move( value ), LuaKind::Thread ) )
{
}
}
