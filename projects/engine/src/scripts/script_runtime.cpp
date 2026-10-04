#include "bubble/scripts/script_runtime.hpp"
#include "bubble/core/asset_path.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/scripts/lua/lua_data.hpp"
#include "bubble/scripts/lua/lua_state.hpp"
#include <algorithm>
#include <format>
#include <lua.h>
#include <lualib.h>
#include <stdexcept>
#include <utility>

#ifdef BUBBLE_LUAU_CODEGEN
#include <luacodegen.h>
#endif

// Script code can create and destroy instances while it runs, so nothing
// here keeps a reference to an entry across a call into Luau: before the
// call the handle is looked up, after it the handle is looked up again.
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
    CurrentScope( ScriptInstanceHandle& current, ScriptInstanceHandle instance )
        : mCurrent( current ),
          mPrevious( current )
    {
        mCurrent = instance;
    }
    ~CurrentScope() { mCurrent = mPrevious; }
    CurrentScope( const CurrentScope& ) = delete;
    CurrentScope& operator=( const CurrentScope& ) = delete;

private:
    ScriptInstanceHandle& mCurrent;
    ScriptInstanceHandle mPrevious;
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

// props { ... } in a library: a library shares through what it returns.
int LibraryProps( lua_State* L )
{
    luaL_error( L, "props belong to entity scripts; a library shares through what it returns" );
}

// "a, b, c", or "none".
string Join( const vector<string>& names )
{
    string list;
    for ( const string& name : names )
        list += ( list.empty() ? "" : ", " ) + name;
    return list.empty() ? "none" : list;
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

void PushHandle( lua_State* L, ScriptInstanceHandle handle )
{
    lua_pushnumber( L, handle.mIndex );
    lua_pushnumber( L, handle.mGeneration );
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

    static ScriptInstanceHandle Current( lua_State* L, const char* what )
    {
        const ScriptInstanceHandle current = Of( L ).mCurrent;
        if ( not current )
            luaL_error( L, "%s works in an entity's callbacks, not at the top of a file", what );
        return current;
    }

    // `L` is the thread running now.
    static void Fail( ScriptRuntime& runtime, ScriptInstanceHandle handle, lua_State* L, const ScriptError& error )
    {
        auto instance = runtime.mInstances.Get( handle );
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
    static bool Resume( ScriptRuntime& runtime, ScriptInstanceHandle handle, lua_State* from, lua_State* thread, int nargs )
    {
        int status = LUA_OK;
        {
            const CurrentScope scope( runtime.mCurrent, handle );
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
        Fail( runtime, handle, from, error );
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
    static void Start( ScriptRuntime& runtime, ScriptInstanceHandle handle, lua_State* L, int function, int nargs )
    {
        auto instance = runtime.mInstances.Get( handle );
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

        AfterResume( L, task, thread, Resume( runtime, handle, L, thread, nargs ) );
        lua_settop( L, top );
    }

    static void Tick( ScriptRuntime& runtime, ScriptInstanceHandle handle, f32 dt )
    {
        lua_State* L = runtime.L();
        const auto enabled = [&] { return runtime.Enabled( handle ); };
        if ( not enabled() )
            return;
        const int top = lua_gettop( L );
        // On the stack, the table outlives the instance if a coroutine
        // destroys it.
        runtime.mInstances.Get( handle )->mTasks.Push( L );
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
                    const CurrentScope scope( runtime.mCurrent, handle );
                    polled = PCall( L, 0, 1 );
                }
                if ( not polled )
                {
                    Fail( runtime, handle, L, polled.error() );
                    break;
                }
                ready = lua_toboolean( L, -1 );
                lua_pop( L, 1 );
            }
            if ( ready )
                AfterResume( L, task, thread, Resume( runtime, handle, L, thread, 0 ) );
            lua_settop( L, task - 1 );
        }
        lua_settop( L, top );
        Sweep( runtime, handle );
    }

    // Drops the finished coroutines, keeping the order of the rest.
    static void Sweep( ScriptRuntime& runtime, ScriptInstanceHandle handle )
    {
        auto instance = runtime.mInstances.Get( handle );
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

    static void Unsubscribe( ScriptRuntime& runtime, ScriptInstanceHandle handle, const Instance& instance )
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
                const bool mine = lua_tonumber( L, -2 ) == handle.mIndex and lua_tonumber( L, -1 ) == handle.mGeneration;
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
            const ScriptInstanceHandle handle{ static_cast<u32>( lua_tonumber( L, -2 ) ),
                                       static_cast<u32>( lua_tonumber( L, -1 ) ) };
            lua_pop( L, 2 );
            if ( runtime.Enabled( handle ) )
            {
                lua_rawgeti( L, -1, 3 );
                runtime.mInstances.Get( handle )->mSelf.Push( L );
                for ( int arg = 0; arg < nargs; ++arg )
                    lua_pushvalue( L, first + arg );
                expected<void, ScriptError> called;
                {
                    const CurrentScope scope( runtime.mCurrent, handle );
                    called = PCall( L, nargs + 1, 0 );
                }
                if ( not called )
                    Fail( runtime, handle, L, called.error() );
            }
            lua_pop( L, 1 );
        }
        lua_settop( L, top );
    }

    // What running a file leaves: its environment, and for a script its
    // props and callbacks, for a library the value it returned.
    struct FileResult
    {
        LuaRef mEnvironment;
        LuaRef mProps;
        vector<LuaRef> mCallbacks;
        LuaRef mResult;
    };

    // Runs a file in a new environment on L - the main thread, or the
    // thread whose require got here.
    static expected<FileResult, ScriptError> RunFile( ScriptRuntime& runtime, lua_State* L, string_view path,
                                                      string_view bytecode, bool library )
    {
        const int top = lua_gettop( L );
        const auto fail = [&]( string message ) -> expected<FileResult, ScriptError> {
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
        if ( library )
            lua_pushcfunction( L, LibraryProps, "props" );
        else
        {
            lua_pushvalue( L, env );
            lua_pushcclosure( L, DeclareProps, "props", 1 );
        }
        lua_rawsetfield( L, env, "props" );
        // require resolves relative paths from this file.
        lua_pushlightuserdata( L, &runtime );
        lua_pushlstring( L, path.data(), path.size() );
        lua_pushcclosure( L, &Impl::LuaRequire, "require", 2 );
        lua_rawsetfield( L, env, "require" );

        if ( auto loaded = LoadScript( L, path, bytecode, env ); not loaded )
            return fail( loaded.error() );
        // The engine's globals are read-only, so lookups through to them
        // may be cached - what lets math.sqrt and friends run as builtins.
        lua_setsafeenv( L, env, true );
#ifdef BUBBLE_LUAU_CODEGEN
        if ( runtime.mState.NativeCode() )
            luau_codegen_compile( L, -1 );
#endif
        runtime.mRunning.emplace_back( path );
        auto ran = PCall( L, 0, library ? 1 : 0 );
        runtime.mRunning.pop_back();
        if ( not ran )
        {
            lua_settop( L, top );
            return std::unexpected( std::move( ran.error() ) );
        }

        // From here on a new global is a mistake.
        lua_pushcfunction( L, UndeclaredGlobal, "undeclared_global" );
        lua_setfield( L, meta, "__newindex" );
        lua_setreadonly( L, meta, true );

        FileResult result;
        if ( library )
        {
            if ( lua_isnil( L, -1 ) )
                return fail( string( path ) + ": a library returns what it shares - end it with return" );
            result.mResult = LuaRef( L, -1 );
            for ( const string& name : runtime.mCallbacks )
            {
                lua_rawgetfield( L, env, name.c_str() );
                const bool defined = not lua_isnil( L, -1 );
                lua_pop( L, 1 );
                if ( defined )
                    return fail( std::format(
                        "{}: {} is a callback of entity scripts; a library only returns what it shares", path,
                        name ) );
            }
        }
        else
        {
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
                return fail( string( path ) + ": " + checked.error() );
            result.mProps = LuaRef( L, props );

            for ( const string& name : runtime.mCallbacks )
            {
                lua_rawgetfield( L, env, name.c_str() );
                if ( lua_isfunction( L, -1 ) )
                    result.mCallbacks.emplace_back( L, -1 );
                else if ( lua_isnil( L, -1 ) )
                    result.mCallbacks.emplace_back();
                else
                    return fail(
                        std::format( "{}: {} is a {}, not a function", path, name, luaL_typename( L, -1 ) ) );
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
                        LogWarning( "{}: {} is not a callback the engine calls ({})", path, name,
                                    Join( runtime.mCallbacks ) );
                }
                lua_pop( L, 1 );
            }
        }

        result.mEnvironment = LuaRef( L, env );
        lua_settop( L, top );
        return result;
    }

    // RunFile, recording what the file requires. A file that fails keeps
    // the record of its last good run, as it keeps its code.
    static expected<FileResult, ScriptError> RunTracked( ScriptRuntime& runtime, lua_State* L, string_view path,
                                                         string_view bytecode, bool library )
    {
        str_hset previous = std::exchange( runtime.mRequires[string( path )], {} );
        auto ran = RunFile( runtime, L, path, bytecode, library );
        if ( not ran )
            runtime.mRequires[string( path )] = std::move( previous );
        return ran;
    }

    // `request` as require reads it in the file `from`: the library's path
    // from the project's root.
    static expected<string, string> Resolve( const ScriptRuntime& runtime, string_view from, string_view request )
    {
        string joined;
        if ( request.starts_with( "./" ) or request.starts_with( "../" ) )
        {
            const size_t slash = from.rfind( '/' );
            const string_view directory = slash == string_view::npos ? string_view() : from.substr( 0, slash );
            joined = string( directory ) + "/" + string( request );
        }
        else if ( request.starts_with( '@' ) )
        {
            const size_t slash = request.find( '/' );
            const string_view alias =
                request.substr( 1, slash == string_view::npos ? string_view::npos : slash - 1 );
            const auto found = runtime.mAliases.find( alias );
            if ( found == runtime.mAliases.end() )
            {
                vector<string> known;
                for ( const auto& entry : runtime.mAliases )
                    known.push_back( "@" + entry.first );
                std::ranges::sort( known );
                return std::unexpected( std::format( "no alias @{} (aliases: {})", alias, Join( known ) ) );
            }
            joined = found->second + ( slash == string_view::npos ? "" : string( request.substr( slash ) ) );
        }
        else
            return std::unexpected( "a path starts with ./, ../ or @alias"s );

        // "." steps go, ".." takes one back.
        vector<string_view> steps;
        const string_view all = joined;
        for ( size_t at = 0; at <= all.size(); )
        {
            const size_t end = std::min( all.find( '/', at ), all.size() );
            const string_view step = all.substr( at, end - at );
            at = end + 1;
            if ( step.empty() or step == "." )
                continue;
            if ( step == ".." )
            {
                if ( steps.empty() )
                    return std::unexpected( "the path leads out of the project"s );
                steps.pop_back();
            }
            else
                steps.push_back( step );
        }
        string path;
        for ( const string_view step : steps )
            path += ( path.empty() ? "" : "/" ) + string( step );
        path += ".luau";
        if ( auto valid = AssetPath::From( path ); not valid )
            return std::unexpected( valid.error() );
        return path;
    }

    // Pushes the value of the library `request` names, running it at its
    // first require.
    static expected<void, string> Require( ScriptRuntime& runtime, lua_State* L, const string& from,
                                           const string& request )
    {
        auto path = Resolve( runtime, from, request );
        if ( not path )
            return std::unexpected( std::format( "require( '{}' ): {}", request, path.error() ) );
        auto found = runtime.mLibraries.find( *path );
        if ( found == runtime.mLibraries.end() )
        {
            // First require of this file in the world: the registry has it
            // loaded, or the world was started without it.
            AssetHandle<ScriptAsset> asset = runtime.mAssets.Find<ScriptAsset>( *AssetPath::From( *path ) );
            if ( not asset.Ready() )
                return std::unexpected( std::format( "require( '{}' ): no library {} is loaded", request, *path ) );
            found = runtime.mLibraries.emplace( *path, Library{ std::move( asset ), {}, {} } ).first;
        }
        runtime.mRequires[from].insert( *path );
        if ( not found->second.mResult.Empty() )
        {
            found->second.mResult.Push( L );
            return {};
        }

        const auto running = std::ranges::find( runtime.mRunning, *path );
        if ( running != runtime.mRunning.end() )
        {
            string chain;
            for ( auto file = running; file != runtime.mRunning.end(); ++file )
                chain += *file + " -> ";
            return std::unexpected( "require cycle: " + chain + *path );
        }

        // The handle keeps the bytecode alive while it runs, even if the
        // library is reloaded meanwhile.
        const AssetHandle<ScriptAsset> asset = found->second.mAsset;
        auto ran = RunTracked( runtime, L, *path, asset.Get()->mBytecode, true );
        if ( not ran )
        {
            const ScriptError& error = ran.error();
            return std::unexpected( error.mTraceback.empty() ? error.mMessage
                                                               : error.mMessage + "\n" + error.mTraceback );
        }
        Library& library = runtime.mLibraries.find( *path )->second;
        library.mResult = std::move( ran->mResult );
        library.mEnvironment = std::move( ran->mEnvironment );
        library.mResult.Push( L );
        return {};
    }

    static int LuaRequire( lua_State* L )
    {
        ScriptRuntime& runtime = Of( L );
        const string from = lua_tostring( L, lua_upvalueindex( 2 ) );
        size_t length = 0;
        const char* text = luaL_checklstring( L, 1, &length );
        const string request( text, length );
        if ( auto pushed = Require( runtime, L, from, request ); not pushed )
            luaL_error( L, "%s", pushed.error().c_str() );
        return 1;
    }

    static void Fill( Module& module, FileResult result )
    {
        module.mEnvironment = std::move( result.mEnvironment );
        module.mProps = std::move( result.mProps );
        module.mCallbacks = std::move( result.mCallbacks );
    }

    static int LuaStart( lua_State* L )
    {
        const ScriptInstanceHandle handle = Current( L, "start" );
        luaL_checktype( L, 1, LUA_TFUNCTION );
        Start( Of( L ), handle, L, 1, lua_gettop( L ) - 1 );
        return 0;
    }

    static int LuaOn( lua_State* L )
    {
        const ScriptInstanceHandle handle = Current( L, "on" );
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
        PushHandle( L, handle );
        lua_rawseti( L, -3, 2 );
        lua_rawseti( L, -2, 1 );
        lua_pushvalue( L, 2 );
        lua_rawseti( L, -2, 3 );
        lua_rawseti( L, -2, lua_objlen( L, -2 ) + 1 );
        runtime.mInstances.Get( handle )->mEvents.insert( event );
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

ScriptRuntime::ScriptRuntime( LuaState& state, AssetRegistry& assets, vector<string> callbacks )
    : mState( state ),
      mAssets( assets ),
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

    mListener = mAssets.OnChanged( [this]( const AssetSlotBase& slot ) { Changed( slot ); } );
}

ScriptRuntime::~ScriptRuntime()
{
    mAssets.RemoveListener( mListener );
}

lua_State* ScriptRuntime::L() const
{
    return mState.L();
}

expected<void, string> ScriptRuntime::SetAlias( string_view name, string_view directory )
{
    const bool named = not name.empty() and std::ranges::all_of( name, []( char c ) {
        return ( c >= 'a' and c <= 'z' ) or ( c >= '0' and c <= '9' ) or c == '_' or c == '-';
    } );
    if ( not named )
        return std::unexpected( std::format( "alias '{}': lowercase letters, digits, _ and - only", name ) );
    string path;
    if ( not directory.empty() )
    {
        auto valid = AssetPath::From( directory );
        if ( not valid )
            return std::unexpected( std::format( "alias @{}: {}", name, valid.error() ) );
        path = valid->String();
    }
    mAliases[string( name )] = std::move( path );
    return {};
}

void ScriptRuntime::Changed( const AssetSlotBase& slot )
{
    const string& path = slot.Path().String();
    // Everything that holds a value of the old file: the library itself,
    // the libraries that required it, theirs in turn.
    str_hset changed;
    if ( mLibraries.contains( path ) )
        changed.insert( path );
    const auto touches = [&]( const str_hset& required ) {
        return std::ranges::any_of( required, [&]( const string& library ) { return changed.contains( library ); } );
    };
    for ( bool grew = not changed.empty(); grew; )
    {
        grew = false;
        for ( const auto& [file, required] : mRequires )
            if ( mLibraries.contains( file ) and not changed.contains( file ) and touches( required ) )
                grew = changed.insert( file ).second;
    }
    for ( const string& library : changed )
    {
        Library& entry = mLibraries.find( library )->second;
        entry.mResult.Reset();
        entry.mEnvironment.Reset();
    }

    // Then the scripts: the changed one, and those that required any of
    // the changed libraries. A failure is logged and leaves the old code.
    for ( const ScriptModuleHandle handle : mModules.Handles() )
    {
        const string chunk = mModules.Get( handle )->mChunk;
        const auto required = mRequires.find( chunk );
        if ( chunk != path and ( required == mRequires.end() or not touches( required->second ) ) )
            continue;
        if ( auto rerun = Rerun( handle ); not rerun )
            LogError( "{}\n{}", rerun.error().mMessage, rerun.error().mTraceback );
    }
}

expected<ScriptModuleHandle, ScriptError> ScriptRuntime::Load( string_view path )
{
    if ( const auto known = mModulesByPath.find( path ); known != mModulesByPath.end() )
        return known->second;
    auto asset = AssetPath::From( path );
    if ( not asset )
        return std::unexpected( ScriptError{ std::format( "{}: {}", path, asset.error() ), {} } );
    AssetHandle<ScriptAsset> script = mAssets.Find<ScriptAsset>( *asset );
    if ( not script.Ready() )
        return std::unexpected( ScriptError{ std::format( "{} is not loaded", path ), {} } );

    auto ran = Impl::RunTracked( *this, L(), asset->View(), script.Get()->mBytecode, false );
    if ( not ran )
        return std::unexpected( std::move( ran.error() ) );
    Module module;
    module.mChunk = asset->String();
    module.mAsset = std::move( script );
    Impl::Fill( module, std::move( *ran ) );
    const ScriptModuleHandle handle = mModules.Add( std::move( module ) );
    mModulesByPath[asset->String()] = handle;
    return handle;
}

expected<void, ScriptError> ScriptRuntime::Rerun( ScriptModuleHandle handle )
{
    // Copies held through the run: it may load and unload modules.
    const string chunk = mModules.Get( handle )->mChunk;
    const AssetHandle<ScriptAsset> asset = mModules.Get( handle )->mAsset;
    if ( not asset.Ready() )
        return std::unexpected( ScriptError{ std::format( "{} is not loaded", chunk ), {} } );
    auto ran = Impl::RunTracked( *this, L(), chunk, asset.Get()->mBytecode, false );
    if ( not ran )
        return std::unexpected( std::move( ran.error() ) );
    auto module = mModules.Get( handle );
    if ( not module )
        return std::unexpected( ScriptError{ chunk + " was unloaded while it ran", {} } );
    Impl::Fill( *module, std::move( *ran ) );

    lua_State* L = this->L();
    const int top = lua_gettop( L );
    module->mProps.Push( L );
    for ( const ScriptInstanceHandle instanceHandle : mInstances.Handles() )
    {
        auto instance = mInstances.Get( instanceHandle );
        if ( instance->mModule != handle )
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

void ScriptRuntime::Unload( ScriptModuleHandle handle )
{
    if ( const auto module = mModules.Get( handle ) )
        mModulesByPath.erase( module->mChunk );
    for ( const ScriptInstanceHandle instance : mInstances.Handles() )
        if ( mInstances.Get( instance )->mModule == handle )
            Destroy( instance );
    mModules.Remove( handle );
}

bool ScriptRuntime::Has( ScriptModuleHandle handle, u32 callback ) const
{
    const auto module = mModules.Get( handle );
    return module and callback < module->mCallbacks.size() and not module->mCallbacks[callback].Empty();
}

void ScriptRuntime::PushProps( ScriptModuleHandle handle ) const
{
    if ( const auto module = mModules.Get( handle ) )
        module->mProps.Push( L() );
    else
        lua_pushnil( L() );
}

expected<ScriptInstanceHandle, string> ScriptRuntime::Create( ScriptModuleHandle moduleHandle, string label, int overrides )
{
    const auto module = mModules.Get( moduleHandle );
    if ( not module )
        return std::unexpected( label + ": its script was unloaded" );
    lua_State* L = this->L();
    const int top = lua_gettop( L );
    if ( overrides != 0 )
        overrides = lua_absindex( L, overrides );
    const auto fail = [&]( string message ) -> expected<ScriptInstanceHandle, string> {
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
    instance.mModule = moduleHandle;
    instance.mLabel = std::move( label );
    instance.mSelf = LuaRef( L, self );
    lua_newtable( L );
    instance.mTasks = LuaRef( L, -1 );
    lua_settop( L, top );
    return mInstances.Add( std::move( instance ) );
}

void ScriptRuntime::Destroy( ScriptInstanceHandle handle )
{
    const auto instance = mInstances.Get( handle );
    if ( not instance )
        return;
    Impl::Unsubscribe( *this, handle, *instance );
    // Its coroutines and self go with its references; a callback of its
    // own still running holds them on the stack until it returns.
    mInstances.Remove( handle );
}

bool ScriptRuntime::Alive( ScriptInstanceHandle handle ) const
{
    return mInstances.Alive( handle );
}

bool ScriptRuntime::Enabled( ScriptInstanceHandle handle ) const
{
    const auto instance = mInstances.Get( handle );
    return instance and instance->mEnabled;
}

void ScriptRuntime::PushSelf( ScriptInstanceHandle handle ) const
{
    if ( const auto instance = mInstances.Get( handle ) )
        instance->mSelf.Push( L() );
    else
        lua_pushnil( L() );
}

expected<void, ScriptError> ScriptRuntime::Call( ScriptInstanceHandle handle, u32 callback, int nargs )
{
    lua_State* L = this->L();
    const auto instance = mInstances.Get( handle );
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
        const CurrentScope scope( mCurrent, handle );
        called = PCall( L, nargs + 1, 0 );
    }
    if ( not called )
        Impl::Fail( *this, handle, L, called.error() );
    return called;
}

void ScriptRuntime::Tick( f32 dt )
{
    BUBBLE_PROFILE_ZONE();
    // An instance made during the tick waits for the next one.
    for ( const ScriptInstanceHandle handle : mInstances.Handles() )
        Impl::Tick( *this, handle, dt );
}

void ScriptRuntime::Emit( string_view event, int nargs )
{
    lua_State* L = this->L();
    Impl::Emit( *this, L, event, lua_gettop( L ) - nargs + 1, nargs );
    lua_pop( L, nargs );
}
}
