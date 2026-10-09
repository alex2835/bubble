#include "luaubind/state.hpp"
#include "luaubind/data.hpp"
#include "luaubind/type.hpp"
#include <iostream>
#include <lua.h>
#include <lualib.h>
#include <stdexcept>

#ifdef LUAUBIND_CODEGEN
#include <luacodegen.h>
#endif

namespace luaubind
{
namespace
{
constexpr size_t cMaxAtoms = 32767;

}

lua_State* detail::SwapActiveThread( lua_State* L )
{
    LuaState& state = LuaState::Of( L );
    lua_State* previous = state.mActive;
    state.mActive = L;
    return previous;
}

// print( ... ): the arguments as tostring shows them, tab-separated, to the
// handler.
int LuaState::Print( lua_State* L )
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
    const LuaState& state = Of( L );
    if ( state.mPrint )
        state.mPrint( text );
    else
        std::cout << text << std::endl;
    return 0;
}

LuaState::LuaState( PrintHandler print ) : mPrint( std::move( print ) )
{
    mL = luaL_newstate();
    mActive = mL;
    lua_Callbacks* callbacks = lua_callbacks( mL );
    callbacks->userdata = this;
    callbacks->useratom = &LuaState::UserAtom;
    luaL_openlibs( mL );
    lua_pushcfunction( mL, &LuaState::Print, "print" );
    lua_setglobal( mL, "print" );
#ifdef LUAUBIND_CODEGEN
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

// ---- values ----------------------------------------------------------------

LuaValue LuaState::Take()
{
    LuaValue value( mActive, -1 );
    lua_pop( mActive, 1 );
    return value;
}

LuaTable LuaState::NewTable()
{
    lua_newtable( mActive );
    return LuaTable( Take() );
}

LuaTable LuaState::NewWeakKeyTable()
{
    lua_newtable( mActive );
    lua_createtable( mActive, 0, 1 );
    lua_pushstring( mActive, "k" );
    lua_setfield( mActive, -2, "__mode" );
    lua_setmetatable( mActive, -2 );
    return LuaTable( Take() );
}

LuaTable LuaState::Globals()
{
    lua_pushvalue( mActive, LUA_GLOBALSINDEX );
    return LuaTable( Take() );
}

string LuaState::Describe( const LuaValue& value )
{
    value.Push( mActive );
    string text = DescribeValue( mActive, -1 );
    lua_pop( mActive, 1 );
    return text;
}

// ---- tables ----------------------------------------------------------------

LuaValue LuaState::GetImpl( const LuaValue& table, const std::function<void( lua_State* )>& pushKey, bool raw )
{
    lua_State* L = mActive;
    const StackRestore restore( L, lua_gettop( L ) );
    table.Push( L );
    pushKey( L );
    if ( raw )
        lua_rawget( L, -2 );
    else
        lua_gettable( L, -2 );
    return LuaValue( L, -1 );
}

void LuaState::SetImpl( const LuaValue& table,
                        const std::function<void( lua_State* )>& pushKey,
                        const std::function<void( lua_State* )>& pushValue,
                        bool raw )
{
    lua_State* L = mActive;
    const StackRestore restore( L, lua_gettop( L ) );
    table.Push( L );
    if ( lua_type( L, -1 ) != LUA_TTABLE )
        throw std::logic_error( "a write to " + DescribeValue( L, -1 ) + ", not a table" );
    // Luau would raise a script error here, outside any protected call.
    if ( lua_getreadonly( L, -1 ) )
        throw std::logic_error( "a write to a frozen table" );
    pushKey( L );
    pushValue( L );
    if ( raw )
        lua_rawset( L, -3 );
    else
        lua_settable( L, -3 );
}

int LuaState::Length( const LuaValue& table )
{
    table.Push( mActive );
    const int length = lua_objlen( mActive, -1 );
    lua_pop( mActive, 1 );
    return length;
}

void LuaState::Append( const LuaValue& table, const LuaValue& value )
{
    lua_State* L = mActive;
    table.Push( L );
    value.Push( L );
    lua_rawseti( L, -2, lua_objlen( L, -2 ) + 1 );
    lua_pop( L, 1 );
}

LuaTable LuaState::Clone( const LuaValue& table )
{
    table.Push( mActive );
    lua_clonetable( mActive, -1 );
    lua_remove( mActive, -2 );
    return LuaTable( Take() );
}

void LuaState::SetMetatable( const LuaValue& table, const LuaValue& metatable )
{
    table.Push( mActive );
    metatable.Push( mActive );
    lua_setmetatable( mActive, -2 );
    lua_pop( mActive, 1 );
}

void LuaState::Freeze( const LuaValue& table )
{
    table.Push( mActive );
    lua_setreadonly( mActive, -1, true );
    lua_pop( mActive, 1 );
}

bool LuaState::Frozen( const LuaValue& table )
{
    table.Push( mActive );
    const bool frozen = lua_getreadonly( mActive, -1 );
    lua_pop( mActive, 1 );
    return frozen;
}

// ---- code ------------------------------------------------------------------

expected<LuaFunction, string> LuaState::Load( string_view chunk, string_view bytecode, const LuaTable& environment )
{
    lua_State* L = mActive;
    const StackRestore restore( L, lua_gettop( L ) );
    environment.Push( L );
    // The environment reads through to the sealed globals: lookups of
    // math.sqrt and friends may be cached, and run as builtins.
    lua_setsafeenv( L, -1, true );
    if ( auto loaded = LoadScript( L, chunk, bytecode, lua_gettop( L ) ); not loaded )
        return std::unexpected( std::move( loaded.error() ) );
#ifdef LUAUBIND_CODEGEN
    if ( mNativeCode )
        luau_codegen_compile( L, -1 );
#endif
    return LuaFunction( LuaValue( L, -1 ) );
}

expected<LuaValue, ScriptError> LuaState::CallImpl( lua_State* L, int top )
{
    const StackRestore restore( L, top );
    if ( auto called = PCall( L, lua_gettop( L ) - top - 1, 1 ); not called )
        return std::unexpected( std::move( called.error() ) );
    return LuaValue( L, -1 );
}

LuaTable LuaState::CallerEnvironment()
{
    lua_State* L = mActive;
    lua_Debug info = {};
    // Level 0 is the C++ running now, level 1 what called it.
    if ( not lua_getinfo( L, 1, "f", &info ) )
        return {};
    lua_getfenv( L, -1 );
    LuaTable environment( LuaValue( L, -1 ) );
    lua_pop( L, 2 );
    return environment;
}

// ---- coroutines ------------------------------------------------------------

LuaThread LuaState::NewThread( const LuaFunction& fn )
{
    lua_State* thread = lua_newthread( mActive );
    fn.Push( thread );
    return LuaThread( Take() );
}

lua_State* LuaState::ThreadOf( const LuaValue& thread )
{
    thread.Push( mActive );
    lua_State* co = lua_tothread( mActive, -1 );
    lua_pop( mActive, 1 );
    if ( co == nullptr )
        throw std::logic_error( "Resume of something that is not a coroutine" );
    return co;
}

LuaResume LuaState::ResumeImpl( lua_State* co, int nargs )
{
    lua_State* from = mActive;
    int status = LUA_OK;
    {
        const detail::ActiveThreadScope active( co );
        status = lua_resume( co, from, nargs );
    }
    LuaResume result;
    if ( status == LUA_YIELD )
    {
        result.mStatus = LuaResume::Status::Waiting;
        result.mWait = LuaValue( co, -1 );
    }
    else if ( status == LUA_OK )
        result.mStatus = LuaResume::Status::Finished;
    else
    {
        result.mStatus = LuaResume::Status::Failed;
        result.mError.mMessage = lua_type( co, -1 ) == LUA_TSTRING ? lua_tostring( co, -1 ) : DescribeValue( co, -1 );
        result.mError.mTraceback = lua_debugtrace( co );
    }
    // What it yielded or returned is taken; the next resume starts clean.
    lua_settop( co, 0 );
    return result;
}

bool LuaState::Yieldable()
{
    return lua_isyieldable( mActive );
}

// ---- data ------------------------------------------------------------------

expected<string, string> LuaState::Encode( const LuaValue& value )
{
    value.Push( mActive );
    auto bytes = LuaEncode( mActive, -1 );
    lua_pop( mActive, 1 );
    return bytes;
}

expected<LuaValue, string> LuaState::Decode( string_view bytes )
{
    if ( auto decoded = LuaDecode( mActive, bytes ); not decoded )
        return std::unexpected( std::move( decoded.error() ) );
    return Take();
}

expected<LuaValue, string> LuaState::DeepCopy( const LuaValue& value )
{
    value.Push( mActive );
    auto copied = LuaDeepCopy( mActive, -1 );
    if ( not copied )
    {
        lua_pop( mActive, 1 );
        return std::unexpected( std::move( copied.error() ) );
    }
    LuaValue copy = Take();
    lua_pop( mActive, 1 );
    return copy;
}

// ---- atoms and types -------------------------------------------------------

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

LuaType* LuaState::FindType( int tag ) const
{
    // Tag 0 is plain userdata; registered types start at 1.
    if ( tag < 1 or static_cast<size_t>( tag ) > mTypes.size() )
        return nullptr;
    return mTypes[static_cast<size_t>( tag - 1 )].get();
}

i16 LuaState::UserAtom( lua_State* L, const char* text, size_t length )
{
    return Of( L ).Atom( string_view( text, length ) );
}
}
