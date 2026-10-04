#include "bubble/scripts/lua/lua_state.hpp"
#include "bubble/core/log.hpp"
#include "bubble/scripts/lua/lua_type.hpp"
#include <lua.h>
#include <lualib.h>

#ifdef BUBBLE_LUAU_CODEGEN
#include <luacodegen.h>
#endif

namespace bubble
{
namespace
{
constexpr size_t cMaxAtoms = 32767;

// print( ... ): the arguments as tostring shows them, tab-separated, into
// the log as a script message.
int Print( lua_State* L )
{
    string text;
    const int count = lua_gettop( L );
    for ( int i = 1; i <= count; ++i )
    {
        size_t length = 0;
        const char* part = luaL_tolstring( L, i, &length );
        if ( i > 1 )
            text += '\t';
        text.append( part, length );
        lua_pop( L, 1 );
    }
    LogMessage( LogLevel::Script, std::move( text ) );
    return 0;
}
}

LuaState::LuaState()
{
    mL = luaL_newstate();
    lua_Callbacks* callbacks = lua_callbacks( mL );
    callbacks->userdata = this;
    callbacks->useratom = &LuaState::UserAtom;
    luaL_openlibs( mL );
    lua_pushcfunction( mL, Print, "print" );
    lua_setglobal( mL, "print" );
#ifdef BUBBLE_LUAU_CODEGEN
    if ( luau_codegen_supported() )
    {
        luau_codegen_create( mL );
        mNativeCode = true;
    }
#endif
}

LuaState::~LuaState()
{
    lua_close( mL );
}

LuaState& LuaState::Of( lua_State* L )
{
    return *static_cast<LuaState*>( lua_callbacks( L )->userdata );
}

void LuaState::Seal()
{
    if ( mSealed )
        return;
    luaL_sandbox( mL );
    mSealed = true;
}

i16 LuaState::Atom( string_view name )
{
    if ( const auto it = mAtoms.find( name ); it != mAtoms.end() )
        return it->second;
    if ( mAtomNames.size() >= cMaxAtoms )
        return -1;
    const auto atom = static_cast<i16>( mAtomNames.size() );
    mAtomNames.emplace_back( name );
    mAtoms.emplace( string( name ), atom );
    return atom;
}

string_view LuaState::AtomName( i16 atom ) const
{
    if ( atom < 0 or static_cast<size_t>( atom ) >= mAtomNames.size() )
        return {};
    return mAtomNames[static_cast<size_t>( atom )];
}

OptRef<LuaType> LuaState::Type( int tag ) const
{
    // Tag 0 is plain userdata; engine types start at 1.
    if ( tag < 1 or static_cast<size_t>( tag ) > mTypes.size() )
        return std::nullopt;
    return *mTypes[static_cast<size_t>( tag - 1 )];
}

i16 LuaState::UserAtom( lua_State* L, const char* text, size_t length )
{
    return Of( L ).Atom( string_view( text, length ) );
}
}
