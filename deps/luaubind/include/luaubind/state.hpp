#pragma once
#include "luaubind/call.hpp"
#include "luaubind/common.hpp"
#include "luaubind/stack.hpp"
#include "luaubind/value.hpp"
#include <stdexcept>
#include <type_traits>

namespace luaubind
{
class LuaType;

// What a coroutine did when resumed.
struct LuaResume
{
    enum class Status
    {
        // Suspended; mWait is what it yielded.
        Waiting,
        Finished,
        // mError says why; the coroutine is dead.
        Failed,
    };
    Status mStatus = Status::Finished;
    LuaValue mWait;
    ScriptError mError;
};

// One Luau VM, and the one way to work with it: everything outside this
// library goes through LuaState and the values it hands out - LuaValue,
// LuaTable, LuaFunction, LuaThread - and never touches the Luau stack.
// Calls come back as expected, with the error and its traceback.
//
// The standard libraries are open; `print` goes to the handler given here
// (stdout without one). The owner adds globals and types, then Seal()
// makes the globals read-only. Where the platform can make machine code,
// loaded code marked @native or --!native runs natively.
//
// Code called from a coroutine runs on the coroutine's thread; LuaState
// follows it (bound functions and Resume switch the active thread), so a
// call made from inside a coroutine lands on the right stack by itself.
class LuaState
{
public:
    using PrintHandler = std::function<void( string_view text )>;

    explicit LuaState( PrintHandler print = {} );
    ~LuaState();
    LuaState( const LuaState& ) = delete;
    LuaState& operator=( const LuaState& ) = delete;

    // ---- the VM ----------------------------------------------------------

    void Seal();
    bool Sealed() const { return mSealed; }
    bool NativeCode() const { return mNativeCode; }

    // The C API, for the binding layer and tests only.
    lua_State* L() const { return mL; }
    // The thread running now: the main one, or a coroutine.
    lua_State* Active() const { return mActive; }
    // The LuaState that owns `L` - its main thread or any coroutine of it.
    static LuaState& Of( lua_State* L );

    // ---- values ----------------------------------------------------------

    LuaTable NewTable();
    // A table whose keys do not keep their values alive: per-object data
    // that goes when the object does.
    LuaTable NewWeakKeyTable();
    // Before Seal, writing here makes a global every script sees.
    LuaTable Globals();

    // Any C++ value LuaTraits knows, a lambda as a function, a LuaValue as
    // it is.
    template <typename T>
    LuaValue Value( const T& value )
    {
        if constexpr ( std::is_base_of_v<LuaValue, T> )
            return value;
        else
        {
            PushValue( mActive, value );
            return Take();
        }
    }

    // `fn` as a Luau function: see LuaPushFunction for what it may take
    // and return.
    template <typename F>
    LuaFunction Function( const char* name, F fn )
    {
        LuaPushFunction( mActive, name, std::move( fn ) );
        return LuaFunction( Take() );
    }

    // As a person reads it in an error: nil, 42, "jump", table.
    string Describe( const LuaValue& value );

    // ---- tables: what LuaTable is made of --------------------------------

    template <typename K>
    LuaValue Get( const LuaValue& table, const K& key )
    {
        return GetImpl( table, [&]( lua_State* L ) { PushValue( L, key ); }, false );
    }
    template <typename K>
    LuaValue RawGet( const LuaValue& table, const K& key )
    {
        return GetImpl( table, [&]( lua_State* L ) { PushValue( L, key ); }, true );
    }
    template <typename K, typename V>
    void Set( const LuaValue& table, const K& key, const V& value )
    {
        SetImpl(
            table, [&]( lua_State* L ) { PushValue( L, key ); }, [&]( lua_State* L ) { PushValue( L, value, key ); },
            false );
    }
    template <typename K, typename V>
    void RawSet( const LuaValue& table, const K& key, const V& value )
    {
        SetImpl(
            table, [&]( lua_State* L ) { PushValue( L, key ); }, [&]( lua_State* L ) { PushValue( L, value, key ); },
            true );
    }

    // Calls fn( key, value ) for every entry; fn may return false to stop.
    // It must not add keys to this table; anything else it may do.
    template <typename F>
    void ForEach( const LuaValue& table, F&& fn )
    {
        lua_State* L = mActive;
        const int top = lua_gettop( L );
        const StackRestore restore( L, top );
        table.Push( L );
        lua_pushnil( L );
        while ( lua_next( L, top + 1 ) )
        {
            LuaValue key( L, -2 );
            LuaValue value( L, -1 );
            lua_pop( L, 1 );
            if constexpr ( std::is_same_v<std::invoke_result_t<F, const LuaValue&, const LuaValue&>, bool> )
            {
                if ( not fn( key, value ) )
                    break;
            }
            else
                fn( key, value );
        }
    }

    int Length( const LuaValue& table );
    void Append( const LuaValue& table, const LuaValue& value );
    LuaTable Clone( const LuaValue& table );
    void SetMetatable( const LuaValue& table, const LuaValue& metatable );
    void Freeze( const LuaValue& table );
    bool Frozen( const LuaValue& table );

    // ---- code ------------------------------------------------------------

    // Bytecode as a function that runs in `environment`. `chunk` names it
    // in errors: "scripts/player.luau", or "=console" for no file.
    // `environment` is promised to read through to the sealed globals, so
    // their lookups may be cached.
    expected<LuaFunction, string> Load( string_view chunk, string_view bytecode, const LuaTable& environment );

    // Calls fn( args... ) - LuaRest arguments spread out - and returns its
    // first result. LuaFunction's call operator is this.
    template <typename... Args>
    expected<LuaValue, ScriptError> Call( const LuaValue& fn, const Args&... args )
    {
        lua_State* L = mActive;
        const int top = lua_gettop( L );
        fn.Push( L );
        ( PushValue( L, args ), ... );
        return CallImpl( L, top );
    }

    // The environment of the Luau function that called the C++ running
    // now: the file it was written in.
    LuaTable CallerEnvironment();

    // ---- coroutines ------------------------------------------------------

    // A coroutine that will run `fn` at its first Resume.
    LuaThread NewThread( const LuaFunction& fn );
    // LuaThread::Resume is this.
    template <typename... Args>
    LuaResume Resume( const LuaValue& thread, const Args&... args )
    {
        lua_State* co = ThreadOf( thread );
        const int before = lua_gettop( co );
        ( PushValue( co, args ), ... );
        return ResumeImpl( co, lua_gettop( co ) - before );
    }
    // A bound function may return LuaYield: it runs inside a coroutine.
    bool Yieldable();

    // ---- data ------------------------------------------------------------

    // Data as bytes and back; see LuaEncode.
    expected<string, string> Encode( const LuaValue& value );
    expected<LuaValue, string> Decode( string_view bytes );
    // A copy all the way down, sharing and cycles kept; see LuaDeepCopy.
    expected<LuaValue, string> DeepCopy( const LuaValue& value );

    // ---- atoms and types -------------------------------------------------

    // A small number for a name: what field and method lookups on
    // registered types switch on instead of comparing strings. Every name
    // Luau asks about gets one in the order first seen - string constants
    // of loaded code too - so types take theirs at registration, before
    // scripts can use the space up. -1 once all 32767 are given out.
    i16 Atom( string_view name );
    // Empty for -1 or a number never given out.
    string_view AtomName( i16 atom ) const;

    // Registered types by their userdata tag, null for none; LuaTypeBuilder
    // adds them.
    LuaType* FindType( int tag ) const;

private:
    friend class LuaType;
    friend struct LuaStateAccess;
    friend lua_State* detail::SwapActiveThread( lua_State* L );

    // Puts the stack back where it was, also when an exception passes.
    class StackRestore
    {
    public:
        StackRestore( lua_State* L, int top ) : mL( L ), mTop( top ) {}
        ~StackRestore() { lua_settop( mL, mTop ); }
        StackRestore( const StackRestore& ) = delete;
        StackRestore& operator=( const StackRestore& ) = delete;

    private:
        lua_State* mL;
        int mTop;
    };

    // A value onto L: what LuaTraits knows, or a lambda or function as a
    // bound function, named after `name` when that is a string.
    template <typename T, typename Name = const char*>
    static void PushValue( lua_State* L, const T& value, const Name& name = "function" )
    {
        if constexpr ( detail::IsCallable<T> )
        {
            if constexpr ( std::is_convertible_v<Name, string_view> )
                LuaPushFunction( L, string( string_view( name ) ).c_str(), value );
            else
                LuaPushFunction( L, "function", value );
        }
        else
            LuaPush( L, value );
    }

    // The value on top of the active thread, popped.
    LuaValue Take();
    LuaValue GetImpl( const LuaValue& table, const std::function<void( lua_State* )>& pushKey, bool raw );
    void SetImpl( const LuaValue& table, const std::function<void( lua_State* )>& pushKey,
                  const std::function<void( lua_State* )>& pushValue, bool raw );
    expected<LuaValue, ScriptError> CallImpl( lua_State* L, int top );
    lua_State* ThreadOf( const LuaValue& thread );
    LuaResume ResumeImpl( lua_State* co, int nargs );

    static int Print( lua_State* L );
    static i16 UserAtom( lua_State* L, const char* text, size_t length );

    lua_State* mL = nullptr;
    lua_State* mActive = nullptr;
    PrintHandler mPrint;
    bool mSealed = false;
    bool mNativeCode = false;
    vector<string> mAtomNames;
    StringMap<i16> mAtoms;
    vector<std::unique_ptr<LuaType>> mTypes;
    // The stack where the last error of a PCall was raised.
    string mLastTraceback;
};

// ---- the value types' templates, which need LuaState -------------------

template <typename T>
opt<T> LuaValue::As() const
{
    if ( IsNil() )
        return std::nullopt;
    lua_State* L = LuaState::Of( mL ).Active();
    Push( L );
    opt<T> result;
    if ( LuaTraits<T>::Is( L, -1 ) )
        result = LuaTraits<T>::Check( L, -1 );
    lua_pop( L, 1 );
    return result;
}

template <typename K>
LuaTableEntry<K> LuaTable::operator[]( K key ) const
{
    return LuaTableEntry<K>( *this, std::move( key ) );
}

template <typename K>
LuaValue LuaTable::Get( const K& key ) const
{
    return IsNil() ? LuaValue() : State()->Get( *this, key );
}

template <typename K>
LuaValue LuaTable::RawGet( const K& key ) const
{
    return IsNil() ? LuaValue() : State()->RawGet( *this, key );
}

template <typename K, typename V>
void LuaTable::Set( const K& key, const V& value ) const
{
    if ( IsNil() )
        throw std::logic_error( "a write to nil, not a table" );
    State()->Set( *this, key, value );
}

template <typename K, typename V>
void LuaTable::RawSet( const K& key, const V& value ) const
{
    if ( IsNil() )
        throw std::logic_error( "a write to nil, not a table" );
    State()->RawSet( *this, key, value );
}

template <typename V>
void LuaTable::Append( const V& value ) const
{
    if ( IsNil() )
        throw std::logic_error( "an append to nil, not a table" );
    State()->Append( *this, State()->Value( value ) );
}

template <typename... Args>
expected<LuaValue, ScriptError> LuaFunction::operator()( const Args&... args ) const
{
    if ( IsNil() )
        return std::unexpected( ScriptError{ "attempt to call nil", {} } );
    return State()->Call( *this, args... );
}

template <typename... Args>
LuaResume LuaThread::Resume( const Args&... args ) const
{
    if ( IsNil() )
    {
        LuaResume dead;
        dead.mStatus = LuaResume::Status::Failed;
        dead.mError.mMessage = "cannot resume nil";
        return dead;
    }
    return State()->Resume( *this, args... );
}
}
