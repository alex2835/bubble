#include "luaubind/type.hpp"

namespace luaubind
{
namespace
{
// The metamethods find their type through upvalue 1.
const LuaType& TypeOf( lua_State* L )
{
    return *static_cast<const LuaType*>( lua_tolightuserdata( L, lua_upvalueindex( 1 ) ) );
}
}

LuaType::LuaType( LuaState& state, string name, int tag, std::type_index type )
    : mState( state ),
      mName( std::move( name ) ),
      mTag( tag ),
      mType( type )
{
}

LuaType& LuaType::Install( LuaState& state, string_view name, std::type_index type, lua_Destructor destroy )
{
    const int tag = static_cast<int>( state.mTypes.size() ) + 1;
    if ( tag >= LUA_UTAG_LIMIT )
        throw std::runtime_error( "too many engine types for Luau's userdata tags" );
    LuaType& luaType = *state.mTypes.emplace_back( std::make_unique<LuaType>( state, string( name ), tag, type ) );

    lua_State* L = state.L();
    lua_setuserdatadtor( L, tag, destroy );

    lua_createtable( L, 0, 5 );
    const auto metamethod = [&]( const char* key, lua_CFunction fn ) {
        lua_pushlightuserdata( L, &luaType );
        lua_pushcclosure( L, fn, key, 1 );
        lua_setfield( L, -2, key );
    };
    metamethod( "__index", &LuaType::Index );
    metamethod( "__newindex", &LuaType::NewIndex );
    metamethod( "__namecall", &LuaType::Namecall );
    // typeof( value ) and error messages name the type.
    lua_pushlstring( L, luaType.mName.data(), luaType.mName.size() );
    lua_setfield( L, -2, "__type" );
    // getmetatable gives the name instead of the table, so scripts cannot
    // reach the metamethods.
    lua_pushlstring( L, luaType.mName.data(), luaType.mName.size() );
    lua_setfield( L, -2, "__metatable" );
    lua_setreadonly( L, -1, true );
    lua_setuserdatametatable( L, tag );
    return luaType;
}

void* LuaType::CheckData( lua_State* L, int index ) const
{
    void* data = lua_touserdatatagged( L, index, mTag );
    if ( data == nullptr )
        luaL_error( L, "%s expected, got %s", mName.c_str(), luaL_typename( L, index ) );
    return data;
}

LuaType::Member& LuaType::Add( string_view name )
{
    const i16 atom = mState.Atom( name );
    if ( atom >= 0 )
    {
        const auto slot = static_cast<size_t>( atom );
        if ( mByAtom.size() <= slot )
            mByAtom.resize( slot + 1, -1 );
        mByAtom[slot] = static_cast<i16>( mMembers.size() );
    }
    else
        mMissingAtoms = true;
    return mMembers.emplace_back( Member{ string( name ), {}, {}, {} } );
}

void LuaType::AddField( string_view name, Getter get, Setter set )
{
    Member& member = Add( name );
    member.mGet = std::move( get );
    member.mSet = std::move( set );
}

void LuaType::AddMethod( string_view name, Method method )
{
    Add( name ).mMethod = std::move( method );
}

const LuaType::Member* LuaType::Find( int atom, string_view name ) const
{
    if ( atom >= 0 )
    {
        const auto slot = static_cast<size_t>( atom );
        if ( slot < mByAtom.size() and mByAtom[slot] >= 0 )
            return &mMembers[static_cast<size_t>( mByAtom[slot] )];
        if ( not mMissingAtoms )
            return nullptr;
    }
    // Atoms ran out before some member was added: those go by name.
    for ( const Member& member : mMembers )
        if ( member.mName == name )
            return &member;
    return nullptr;
}

void LuaType::NoMember( lua_State* L, string_view name, bool method ) const
{
    string known;
    for ( const Member& member : mMembers )
    {
        if ( ( member.mMethod != nullptr ) != method )
            continue;
        if ( not known.empty() )
            known += ", ";
        known += member.mName;
    }
    if ( known.empty() )
        known = "none";
    luaL_error( L, "%s has no %s '%.*s' (%s: %s)", mName.c_str(), method ? "method" : "field",
                static_cast<int>( name.size() ), name.data(), method ? "methods" : "fields", known.c_str() );
}

int LuaType::Index( lua_State* L )
{
    const LuaType& type = TypeOf( L );
    void* data = type.CheckData( L, 1 );
    int atom = -1;
    size_t length = 0;
    const char* key = lua_tolstringatom( L, 2, &length, &atom );
    if ( key == nullptr )
        luaL_error( L, "%s fields are named, not %s", type.mName.c_str(), luaL_typename( L, 2 ) );
    const string_view name( key, length );
    const auto member = type.Find( atom, name );
    if ( not member )
        type.NoMember( L, name, false );
    if ( not member->mGet )
        luaL_error( L, "%s:%s is a method: call it with ':'", type.mName.c_str(), key );
    member->mGet( L, data );
    return 1;
}

int LuaType::NewIndex( lua_State* L )
{
    const LuaType& type = TypeOf( L );
    void* data = type.CheckData( L, 1 );
    int atom = -1;
    size_t length = 0;
    const char* key = lua_tolstringatom( L, 2, &length, &atom );
    if ( key == nullptr )
        luaL_error( L, "%s fields are named, not %s", type.mName.c_str(), luaL_typename( L, 2 ) );
    const string_view name( key, length );
    const auto member = type.Find( atom, name );
    if ( not member or not member->mGet )
        type.NoMember( L, name, false );
    if ( not member->mSet )
        luaL_error( L, "%s.%s is read-only", type.mName.c_str(), key );
    member->mSet( L, data, 3 );
    return 0;
}

int LuaType::Namecall( lua_State* L )
{
    const LuaType& type = TypeOf( L );
    void* data = type.CheckData( L, 1 );
    int atom = -1;
    const char* key = lua_namecallatom( L, &atom );
    const string_view name = key != nullptr ? string_view( key ) : string_view();
    const auto member = type.Find( atom, name );
    if ( not member or not member->mMethod )
        type.NoMember( L, name, true );
    return member->mMethod( L, data );
}
}
