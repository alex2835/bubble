#include "bubble/scripts/script_runtime.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/scripts/lua/lua_data.hpp"
#include "bubble/scripts/lua/lua_state.hpp"
#include <algorithm>
#include <format>
#include <lua.h>
#include <lualib.h>
#include <stdexcept>

#ifdef BUBBLE_LUAU_CODEGEN
#include <luacodegen.h>
#endif

// Script code can create and destroy instances while it runs, so nothing
// here keeps a reference to an entry across a call into Luau: before the
// call the id is looked up, after it the id is looked up again.
namespace bubble
{
namespace
{
// The key the file's props table is kept under in its environment: a
// light userdata, which no script can spell.
char sPropsKey = 0;

// Swaps in the instance whose code runs, and back.
class CurrentScope
{
public:
    CurrentScope( ScriptInstanceId& current, ScriptInstanceId instance )
        : mCurrent( current ),
          mPrevious( current )
    {
        mCurrent = instance;
    }
    ~CurrentScope() { mCurrent = mPrevious; }
    CurrentScope( const CurrentScope& ) = delete;
    CurrentScope& operator=( const CurrentScope& ) = delete;

private:
    ScriptInstanceId& mCurrent;
    ScriptInstanceId mPrevious;
};

// props { ... }: upvalue 1 is the file's environment.
int DeclareProps( lua_State* L )
{
    luaL_checktype( L, 1, LUA_TTABLE );
    lua_rawgetp( L, lua_upvalueindex( 1 ), &sPropsKey );
    if ( not lua_isnil( L, -1 ) )
        luaL_error( L, "props is declared once, at the top of the file" );
    lua_pop( L, 1 );
    lua_pushvalue( L, 1 );
    lua_rawsetp( L, lua_upvalueindex( 1 ), &sPropsKey );
    return 0;
}

// prop( default, info ): the hints are for the inspector and are read on
// import; at run time a prop is its default.
int Prop( lua_State* L )
{
    luaL_checkany( L, 1 );
    if ( not lua_isnoneornil( L, 2 ) )
        luaL_checktype( L, 2, LUA_TTABLE );
    lua_settop( L, 1 );
    return 1;
}

int UndeclaredGlobal( lua_State* L )
{
    luaL_error( L, "assignment to undeclared global '%s': declare it at the top of the file, or keep it in self",
                luaL_tolstring( L, 2, nullptr ) );
}

int Wait( lua_State* L )
{
    if ( not lua_isyieldable( L ) )
        luaL_error( L, "wait works inside start( fn ), not in a callback" );
    const double seconds = luaL_optnumber( L, 1, 0.0 );
    lua_settop( L, 0 );
    lua_pushnumber( L, seconds );
    return lua_yield( L, 1 );
}

int WaitUntil( lua_State* L )
{
    if ( not lua_isyieldable( L ) )
        luaL_error( L, "wait_until works inside start( fn ), not in a callback" );
    luaL_checktype( L, 1, LUA_TFUNCTION );
    lua_settop( L, 1 );
    return lua_yield( L, 1 );
}

// The names of a table's string keys, sorted: "jump_height, speed".
string KeyList( lua_State* L, int table )
{
    vector<string> names;
    lua_pushnil( L );
    while ( lua_next( L, table ) )
    {
        if ( lua_type( L, -2 ) == LUA_TSTRING )
            names.emplace_back( lua_tostring( L, -2 ) );
        lua_pop( L, 1 );
    }
    std::ranges::sort( names );
    string list;
    for ( const string& name : names )
        list += ( list.empty() ? "" : ", " ) + name;
    return list.empty() ? "none" : list;
}

// The props at `props` are data a scene file can hold, under names self
// leaves free.
expected<void, string> CheckProps( lua_State* L, int props )
{
    lua_pushnil( L );
    while ( lua_next( L, props ) )
    {
        if ( lua_type( L, -2 ) != LUA_TSTRING )
        {
            string error = "props are named, " + DescribeValue( L, -2 ) + " is not a name";
            lua_pop( L, 2 );
            return std::unexpected( std::move( error ) );
        }
        const string name = lua_tostring( L, -2 );
        if ( name == "entity" )
        {
            lua_pop( L, 2 );
            return std::unexpected( "a prop cannot be called 'entity': self.entity is the engine's"s );
        }
        if ( auto encoded = LuaEncode( L, -1 ); not encoded )
        {
            lua_pop( L, 2 );
            return std::unexpected( "prop " + name + ": " + encoded.error() );
        }
        lua_pop( L, 1 );
    }
    return {};
}

// self[key] = a copy of value for every pair of the table at `from`; with
// `onlyMissing`, keys self has already are left alone.
expected<void, string> CopyInto( lua_State* L, int self, int from, bool onlyMissing )
{
    lua_pushnil( L );
    while ( lua_next( L, from ) )
    {
        lua_pushvalue( L, -2 );
        lua_rawget( L, self );
        const bool present = not lua_isnil( L, -1 );
        lua_pop( L, 1 );
        if ( not ( onlyMissing and present ) )
        {
            lua_pushvalue( L, -2 );
            if ( auto copied = LuaDeepCopy( L, -2 ); not copied )
            {
                lua_pop( L, 3 );
                return copied;
            }
            lua_rawset( L, self );
        }
        lua_pop( L, 1 );
    }
    return {};
}

void PushId( lua_State* L, ScriptInstanceId id )
{
    lua_pushnumber( L, id.mIndex );
    lua_pushnumber( L, id.mGeneration );
}
}

// What the Luau functions and the public calls share, with the runtime's
// private parts in reach.
struct ScriptRuntime::Impl
{
    static ScriptRuntime& Of( lua_State* L )
    {
        return *static_cast<ScriptRuntime*>( lua_tolightuserdata( L, lua_upvalueindex( 1 ) ) );
    }

    static ScriptInstanceId Current( lua_State* L, const char* what )
    {
        const ScriptInstanceId current = Of( L ).mCurrent;
        if ( not current )
            luaL_error( L, "%s works in an entity's callbacks, not at the top of a file", what );
        return current;
    }

    // `L` is the thread running now.
    static void Fail( ScriptRuntime& runtime, ScriptInstanceId id, lua_State* L, const ScriptError& error )
    {
        auto instance = runtime.mInstances.Get( id );
        if ( not instance )
            return;
        LogError( "{}: {}\n{}", instance->mLabel, error.mMessage, error.mTraceback );
        instance->mEnabled = false;
        // Its coroutines stop with it; a changed file starts it over.
        lua_newtable( L );
        instance->mTasks = LuaRef( L, -1 );
        lua_pop( L, 1 );
    }

    // Runs the coroutine until it waits (true: the wait is on top of the
    // thread's stack) or ends (false; an error switches the instance off).
    static bool Resume( ScriptRuntime& runtime, ScriptInstanceId id, lua_State* from, lua_State* thread, int nargs )
    {
        int status = LUA_OK;
        {
            const CurrentScope scope( runtime.mCurrent, id );
            status = lua_resume( thread, from, nargs );
        }
        if ( status == LUA_YIELD )
            return true;
        if ( status == LUA_OK )
            return false;
        ScriptError error;
        error.mMessage =
            lua_type( thread, -1 ) == LUA_TSTRING ? lua_tostring( thread, -1 ) : DescribeValue( thread, -1 );
        error.mTraceback = lua_debugtrace( thread );
        Fail( runtime, id, from, error );
        return false;
    }

    // Records in `task` what its coroutine waits on after a Resume.
    static void AfterResume( lua_State* L, int task, lua_State* thread, bool waiting )
    {
        if ( waiting )
        {
            lua_xmove( thread, L, 1 );
            lua_rawseti( L, task, 2 );
        }
        else
        {
            lua_pushboolean( L, false );
            lua_rawseti( L, task, 1 );
        }
    }

    // A coroutine of the function at `function` of L - the thread that
    // called start - with the `nargs` values after it, run to its first wait.
    static void Start( ScriptRuntime& runtime, ScriptInstanceId id, lua_State* L, int function, int nargs )
    {
        auto instance = runtime.mInstances.Get( id );
        if ( not instance )
            return;
        const int top = lua_gettop( L );
        lua_State* thread = lua_newthread( L );
        for ( int i = 0; i <= nargs; ++i )
            lua_xpush( L, thread, function + i );

        // The task keeps the thread alive from before its first step.
        lua_createtable( L, 2, 0 );
        const int task = lua_gettop( L );
        lua_pushvalue( L, top + 1 );
        lua_rawseti( L, task, 1 );
        instance->mTasks.Push( L );
        lua_pushvalue( L, task );
        lua_rawseti( L, -2, lua_objlen( L, -2 ) + 1 );
        lua_pop( L, 1 );

        AfterResume( L, task, thread, Resume( runtime, id, L, thread, nargs ) );
        lua_settop( L, top );
    }

    static void Tick( ScriptRuntime& runtime, ScriptInstanceId id, f32 dt )
    {
        lua_State* L = runtime.L();
        const auto enabled = [&] { return runtime.Enabled( id ); };
        if ( not enabled() )
            return;
        const int top = lua_gettop( L );
        // On the stack, the table outlives the instance if a coroutine
        // destroys it.
        runtime.mInstances.Get( id )->mTasks.Push( L );
        const int tasks = lua_gettop( L );
        // Coroutines started during this tick have run already.
        const int count = lua_objlen( L, tasks );
        for ( int i = 1; i <= count and enabled(); ++i )
        {
            lua_rawgeti( L, tasks, i );
            const int task = lua_gettop( L );
            lua_rawgeti( L, task, 1 );
            lua_State* thread = lua_tothread( L, -1 );
            lua_rawgeti( L, task, 2 );
            bool ready = thread != nullptr;
            if ( ready and lua_type( L, -1 ) == LUA_TNUMBER )
            {
                const double left = lua_tonumber( L, -1 ) - dt;
                ready = left <= 0;
                if ( not ready )
                {
                    lua_pushnumber( L, left );
                    lua_rawseti( L, task, 2 );
                }
            }
            else if ( ready and lua_isfunction( L, -1 ) )
            {
                lua_pushvalue( L, -1 );
                expected<void, ScriptError> polled;
                {
                    const CurrentScope scope( runtime.mCurrent, id );
                    polled = PCall( L, 0, 1 );
                }
                if ( not polled )
                {
                    Fail( runtime, id, L, polled.error() );
                    break;
                }
                ready = lua_toboolean( L, -1 );
                lua_pop( L, 1 );
            }
            if ( ready )
                AfterResume( L, task, thread, Resume( runtime, id, L, thread, 0 ) );
            lua_settop( L, task - 1 );
        }
        lua_settop( L, top );
        Sweep( runtime, id );
    }

    // Drops the finished coroutines, keeping the order of the rest.
    static void Sweep( ScriptRuntime& runtime, ScriptInstanceId id )
    {
        auto instance = runtime.mInstances.Get( id );
        if ( not instance )
            return;
        lua_State* L = runtime.L();
        instance->mTasks.Push( L );
        const int tasks = lua_gettop( L );
        lua_newtable( L );
        int kept = 0;
        const int count = lua_objlen( L, tasks );
        for ( int i = 1; i <= count; ++i )
        {
            lua_rawgeti( L, tasks, i );
            lua_rawgeti( L, -1, 1 );
            const bool running = lua_isthread( L, -1 );
            lua_pop( L, 1 );
            if ( running )
                lua_rawseti( L, tasks + 1, ++kept );
            else
                lua_pop( L, 1 );
        }
        instance->mTasks = LuaRef( L, -1 );
        lua_pop( L, 2 );
    }

    static void Unsubscribe( ScriptRuntime& runtime, ScriptInstanceId id, const Instance& instance )
    {
        lua_State* L = runtime.L();
        runtime.mEvents.Push( L );
        const int events = lua_gettop( L );
        for ( const string& event : instance.mEvents )
        {
            lua_rawgetfield( L, events, event.c_str() );
            const int list = lua_gettop( L );
            lua_newtable( L );
            int kept = 0;
            const int count = lua_objlen( L, list );
            for ( int i = 1; i <= count; ++i )
            {
                lua_rawgeti( L, list, i );
                lua_rawgeti( L, -1, 1 );
                lua_rawgeti( L, -2, 2 );
                const bool mine = lua_tonumber( L, -2 ) == id.mIndex and lua_tonumber( L, -1 ) == id.mGeneration;
                lua_pop( L, 2 );
                if ( mine )
                    lua_pop( L, 1 );
                else
                    lua_rawseti( L, list + 1, ++kept );
            }
            lua_rawsetfield( L, events, event.c_str() );
            lua_pop( L, 1 );
        }
        lua_pop( L, 1 );
    }

    // Calls the subscribers of `event` with the `nargs` values from slot
    // `first` on of L, the thread that emits.
    static void Emit( ScriptRuntime& runtime, lua_State* L, string_view event, int first, int nargs )
    {
        const int top = lua_gettop( L );
        runtime.mEvents.Push( L );
        lua_pushlstring( L, event.data(), event.size() );
        lua_rawget( L, -2 );
        if ( lua_isnil( L, -1 ) )
        {
            lua_settop( L, top );
            return;
        }
        // Subscribing or leaving during the calls changes the list, not
        // this pass over it.
        lua_clonetable( L, -1 );
        const int list = lua_gettop( L );
        const int count = lua_objlen( L, list );
        for ( int i = 1; i <= count; ++i )
        {
            lua_rawgeti( L, list, i );
            lua_rawgeti( L, -1, 1 );
            lua_rawgeti( L, -2, 2 );
            const ScriptInstanceId id{ static_cast<u32>( lua_tonumber( L, -2 ) ),
                                       static_cast<u32>( lua_tonumber( L, -1 ) ) };
            lua_pop( L, 2 );
            if ( runtime.Enabled( id ) )
            {
                lua_rawgeti( L, -1, 3 );
                runtime.mInstances.Get( id )->mSelf.Push( L );
                for ( int arg = 0; arg < nargs; ++arg )
                    lua_pushvalue( L, first + arg );
                expected<void, ScriptError> called;
                {
                    const CurrentScope scope( runtime.mCurrent, id );
                    called = PCall( L, nargs + 1, 0 );
                }
                if ( not called )
                    Fail( runtime, id, L, called.error() );
            }
            lua_pop( L, 1 );
        }
        lua_settop( L, top );
    }

    // Runs a file in a new environment and fills `out` from it.
    static expected<void, ScriptError> Run( ScriptRuntime& runtime, string_view chunk, string_view bytecode,
                                            Module& out )
    {
        lua_State* L = runtime.L();
        const int top = lua_gettop( L );
        const auto fail = [&]( string message ) -> expected<void, ScriptError> {
            lua_settop( L, top );
            return std::unexpected( ScriptError{ std::move( message ), {} } );
        };
        if ( not runtime.mState.Sealed() )
            return fail( "scripts load once the Luau state is sealed" );

        // The file's own globals, reading through to the engine's.
        lua_newtable( L );
        const int env = lua_gettop( L );
        lua_createtable( L, 0, 2 );
        const int meta = lua_gettop( L );
        lua_pushvalue( L, LUA_GLOBALSINDEX );
        lua_setfield( L, meta, "__index" );
        lua_pushvalue( L, meta );
        lua_setmetatable( L, env );
        lua_pushvalue( L, env );
        lua_pushcclosure( L, DeclareProps, "props", 1 );
        lua_rawsetfield( L, env, "props" );

        if ( auto loaded = LoadScript( L, chunk, bytecode, env ); not loaded )
            return fail( loaded.error() );
        // The engine's globals are read-only, so lookups through to them
        // may be cached - what lets math.sqrt and friends run as builtins.
        lua_setsafeenv( L, env, true );
#ifdef BUBBLE_LUAU_CODEGEN
        if ( runtime.mState.NativeCode() )
            luau_codegen_compile( L, -1 );
#endif
        if ( auto ran = PCall( L, 0, 0 ); not ran )
        {
            lua_settop( L, top );
            return std::unexpected( std::move( ran.error() ) );
        }

        // From here on a new global is a mistake.
        lua_pushcfunction( L, UndeclaredGlobal, "undeclared_global" );
        lua_setfield( L, meta, "__newindex" );
        lua_setreadonly( L, meta, true );

        lua_rawgetp( L, env, &sPropsKey );
        if ( lua_isnil( L, -1 ) )
        {
            // None declared: an empty table, which also makes a later
            // props { } an error.
            lua_pop( L, 1 );
            lua_newtable( L );
            lua_pushvalue( L, -1 );
            lua_rawsetp( L, env, &sPropsKey );
        }
        const int props = lua_gettop( L );
        if ( auto checked = CheckProps( L, props ); not checked )
            return fail( string( chunk ) + ": " + checked.error() );

        vector<LuaRef> callbacks;
        for ( const string& name : runtime.mCallbacks )
        {
            lua_rawgetfield( L, env, name.c_str() );
            if ( lua_isfunction( L, -1 ) )
                callbacks.emplace_back( L, -1 );
            else if ( lua_isnil( L, -1 ) )
                callbacks.emplace_back();
            else
                return fail( std::format( "{}: {} is a {}, not a function", chunk, name, luaL_typename( L, -1 ) ) );
            lua_pop( L, 1 );
        }

        // on_updte would never run; say so instead of staying silent.
        lua_pushnil( L );
        while ( lua_next( L, env ) )
        {
            if ( lua_type( L, -2 ) == LUA_TSTRING and lua_isfunction( L, -1 ) )
            {
                const string_view name = lua_tostring( L, -2 );
                if ( name.starts_with( "on_" ) and
                     std::ranges::find( runtime.mCallbacks, name ) == runtime.mCallbacks.end() )
                {
                    string known;
                    for ( const string& callback : runtime.mCallbacks )
                        known += ( known.empty() ? "" : ", " ) + callback;
                    LogWarning( "{}: {} is not a callback the engine calls ({})", chunk, name, known );
                }
            }
            lua_pop( L, 1 );
        }

        out.mChunk = string( chunk );
        out.mEnvironment = LuaRef( L, env );
        out.mProps = LuaRef( L, props );
        out.mCallbacks = std::move( callbacks );
        lua_settop( L, top );
        return {};
    }

    static int LuaStart( lua_State* L )
    {
        const ScriptInstanceId id = Current( L, "start" );
        luaL_checktype( L, 1, LUA_TFUNCTION );
        Start( Of( L ), id, L, 1, lua_gettop( L ) - 1 );
        return 0;
    }

    static int LuaOn( lua_State* L )
    {
        const ScriptInstanceId id = Current( L, "on" );
        const string event = luaL_checkstring( L, 1 );
        luaL_checktype( L, 2, LUA_TFUNCTION );
        ScriptRuntime& runtime = Of( L );
        runtime.mEvents.Push( L );
        const int events = lua_gettop( L );
        lua_rawgetfield( L, events, event.c_str() );
        if ( lua_isnil( L, -1 ) )
        {
            lua_pop( L, 1 );
            lua_newtable( L );
            lua_pushvalue( L, -1 );
            lua_rawsetfield( L, events, event.c_str() );
        }
        lua_createtable( L, 3, 0 );
        PushId( L, id );
        lua_rawseti( L, -3, 2 );
        lua_rawseti( L, -2, 1 );
        lua_pushvalue( L, 2 );
        lua_rawseti( L, -2, 3 );
        lua_rawseti( L, -2, lua_objlen( L, -2 ) + 1 );
        runtime.mInstances.Get( id )->mEvents.insert( event );
        return 0;
    }

    static int LuaEmit( lua_State* L )
    {
        size_t length = 0;
        const char* event = luaL_checklstring( L, 1, &length );
        Emit( Of( L ), L, string_view( event, length ), 2, lua_gettop( L ) - 1 );
        return 0;
    }
};

// ---- ScriptRuntime -------------------------------------------------------

ScriptRuntime::ScriptRuntime( LuaState& state, vector<string> callbacks )
    : mState( state ),
      mCallbacks( std::move( callbacks ) )
{
    if ( state.Sealed() )
        throw std::logic_error( "ScriptRuntime adds globals: build it before the state is sealed" );
    lua_State* L = state.L();
    lua_newtable( L );
    mEvents = LuaRef( L, -1 );
    lua_pop( L, 1 );

    const auto global = [&]( const char* name, lua_CFunction fn ) {
        lua_pushlightuserdata( L, this );
        lua_pushcclosure( L, fn, name, 1 );
        lua_setglobal( L, name );
    };
    global( "prop", Prop );
    global( "start", &Impl::LuaStart );
    global( "wait", Wait );
    global( "wait_until", WaitUntil );
    global( "on", &Impl::LuaOn );
    global( "emit", &Impl::LuaEmit );
}

ScriptRuntime::~ScriptRuntime() = default;

lua_State* ScriptRuntime::L() const
{
    return mState.L();
}

expected<ScriptModuleId, ScriptError> ScriptRuntime::Load( string_view chunk, string_view bytecode )
{
    Module module;
    if ( auto ran = Impl::Run( *this, chunk, bytecode, module ); not ran )
        return std::unexpected( std::move( ran.error() ) );
    return mModules.Add( std::move( module ) );
}

expected<void, ScriptError> ScriptRuntime::Reload( ScriptModuleId id, string_view bytecode )
{
    if ( not mModules.Alive( id ) )
        return std::unexpected( ScriptError{ "the script was unloaded", {} } );
    Module fresh;
    if ( auto ran = Impl::Run( *this, mModules.Get( id )->mChunk, bytecode, fresh ); not ran )
        return ran;
    auto module = mModules.Get( id );
    if ( not module )
        return std::unexpected( ScriptError{ "the script was unloaded while it ran", {} } );
    module->mEnvironment = std::move( fresh.mEnvironment );
    module->mProps = std::move( fresh.mProps );
    module->mCallbacks = std::move( fresh.mCallbacks );

    lua_State* L = this->L();
    const int top = lua_gettop( L );
    module->mProps.Push( L );
    for ( const ScriptInstanceId instanceId : mInstances.Ids() )
    {
        auto instance = mInstances.Get( instanceId );
        if ( instance->mModule != id )
            continue;
        instance->mSelf.Push( L );
        // Props are data and copy; a failure here would have failed Run.
        (void)CopyInto( L, top + 2, top + 1, true );
        lua_pop( L, 1 );
        instance->mEnabled = true;
    }
    lua_settop( L, top );
    return {};
}

void ScriptRuntime::Unload( ScriptModuleId id )
{
    for ( const ScriptInstanceId instance : mInstances.Ids() )
        if ( mInstances.Get( instance )->mModule == id )
            Destroy( instance );
    mModules.Remove( id );
}

bool ScriptRuntime::Has( ScriptModuleId id, u32 callback ) const
{
    const auto module = mModules.Get( id );
    return module and callback < module->mCallbacks.size() and not module->mCallbacks[callback].Empty();
}

void ScriptRuntime::PushProps( ScriptModuleId id ) const
{
    if ( const auto module = mModules.Get( id ) )
        module->mProps.Push( L() );
    else
        lua_pushnil( L() );
}

expected<ScriptInstanceId, string> ScriptRuntime::Create( ScriptModuleId moduleId, string label, int overrides )
{
    const auto module = mModules.Get( moduleId );
    if ( not module )
        return std::unexpected( label + ": its script was unloaded" );
    lua_State* L = this->L();
    const int top = lua_gettop( L );
    if ( overrides != 0 )
        overrides = lua_absindex( L, overrides );
    const auto fail = [&]( string message ) -> expected<ScriptInstanceId, string> {
        lua_settop( L, top );
        return std::unexpected( std::move( message ) );
    };

    module->mProps.Push( L );
    const int props = top + 1;
    lua_newtable( L );
    const int self = top + 2;
    // Each instance gets its own copy: a table among the props is not
    // shared between entities.
    if ( auto copied = CopyInto( L, self, props, false ); not copied )
        return fail( label + ": " + copied.error() );

    if ( overrides != 0 )
    {
        lua_pushnil( L );
        while ( lua_next( L, overrides ) )
        {
            lua_pushvalue( L, -2 );
            lua_rawget( L, props );
            if ( lua_isnil( L, -1 ) or lua_type( L, -3 ) != LUA_TSTRING )
                return fail( std::format( "{}: {} is not a prop of {} (props: {})", label, DescribeValue( L, -3 ),
                                          module->mChunk, KeyList( L, props ) ) );
            if ( lua_type( L, -1 ) != lua_type( L, -2 ) )
                return fail( std::format( "{}: prop {} is a {} in {}, the override is a {}", label,
                                          lua_tostring( L, -3 ), luaL_typename( L, -1 ), module->mChunk,
                                          luaL_typename( L, -2 ) ) );
            lua_pop( L, 1 );
            lua_pushvalue( L, -2 );
            if ( auto copied = LuaDeepCopy( L, -2 ); not copied )
                return fail( label + ": " + copied.error() );
            lua_rawset( L, self );
            lua_pop( L, 1 );
        }
    }

    Instance instance;
    instance.mModule = moduleId;
    instance.mLabel = std::move( label );
    instance.mSelf = LuaRef( L, self );
    lua_newtable( L );
    instance.mTasks = LuaRef( L, -1 );
    lua_settop( L, top );
    return mInstances.Add( std::move( instance ) );
}

void ScriptRuntime::Destroy( ScriptInstanceId id )
{
    const auto instance = mInstances.Get( id );
    if ( not instance )
        return;
    Impl::Unsubscribe( *this, id, *instance );
    // Its coroutines and self go with its references; a callback of its
    // own still running holds them on the stack until it returns.
    mInstances.Remove( id );
}

bool ScriptRuntime::Alive( ScriptInstanceId id ) const
{
    return mInstances.Alive( id );
}

bool ScriptRuntime::Enabled( ScriptInstanceId id ) const
{
    const auto instance = mInstances.Get( id );
    return instance and instance->mEnabled;
}

void ScriptRuntime::PushSelf( ScriptInstanceId id ) const
{
    if ( const auto instance = mInstances.Get( id ) )
        instance->mSelf.Push( L() );
    else
        lua_pushnil( L() );
}

expected<void, ScriptError> ScriptRuntime::Call( ScriptInstanceId id, u32 callback, int nargs )
{
    lua_State* L = this->L();
    const auto instance = mInstances.Get( id );
    if ( not instance )
    {
        lua_pop( L, nargs );
        return std::unexpected( ScriptError{ "the script instance was destroyed", {} } );
    }
    if ( not instance->mEnabled or not Has( instance->mModule, callback ) )
    {
        lua_pop( L, nargs );
        return {};
    }
    const int base = lua_gettop( L ) - nargs;
    mModules.Get( instance->mModule )->mCallbacks[callback].Push( L );
    lua_insert( L, base + 1 );
    instance->mSelf.Push( L );
    lua_insert( L, base + 2 );

    expected<void, ScriptError> called;
    {
        const CurrentScope scope( mCurrent, id );
        called = PCall( L, nargs + 1, 0 );
    }
    if ( not called )
        Impl::Fail( *this, id, L, called.error() );
    return called;
}

void ScriptRuntime::Tick( f32 dt )
{
    BUBBLE_PROFILE_ZONE();
    // An instance made during the tick waits for the next one.
    for ( const ScriptInstanceId id : mInstances.Ids() )
        Impl::Tick( *this, id, dt );
}

void ScriptRuntime::Emit( string_view event, int nargs )
{
    lua_State* L = this->L();
    Impl::Emit( *this, L, event, lua_gettop( L ) - nargs + 1, nargs );
    lua_pop( L, nargs );
}
}
