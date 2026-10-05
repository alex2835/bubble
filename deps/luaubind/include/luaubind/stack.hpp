#pragma once
#include "luaubind/common.hpp"
#include "luaubind/value.hpp"
#include <cmath>
#include <concepts>
#include <limits>
#include <lua.h>
#include <lualib.h>
#include <new>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

// How C++ values cross into Luau and back, and C++ functions bound as Luau
// functions with their arguments checked and converted. The set of types is
// closed on purpose: what is not here does not cross.
namespace luaubind
{
static_assert( LUA_VECTOR_SIZE == 3, "vectors cross as three floats" );

// A three-float vector - glm::vec3 and the like - crosses as Luau's own
// vector, which costs no allocation.
template <typename T>
concept LuaVectorLike = requires( T v ) {
    requires std::same_as<decltype( v.x ), f32>;
    requires std::same_as<decltype( v.y ), f32>;
    requires std::same_as<decltype( v.z ), f32>;
} and sizeof( T ) == 3 * sizeof( f32 );

// Per type: its name for error messages, Is (the value at `index` is one),
// Push, and Check (the value at `index` converted, or a script error naming
// the argument).
template <typename T>
struct LuaTraits;

template <>
struct LuaTraits<bool>
{
    static constexpr const char* cName = "boolean";
    static bool Is( lua_State* L, int index ) { return lua_isboolean( L, index ); }
    static void Push( lua_State* L, bool value ) { lua_pushboolean( L, value ); }
    static bool Check( lua_State* L, int index ) { return luaL_checkboolean( L, index ); }
};

template <std::floating_point T>
struct LuaTraits<T>
{
    static constexpr const char* cName = "number";
    static bool Is( lua_State* L, int index ) { return lua_type( L, index ) == LUA_TNUMBER; }
    static void Push( lua_State* L, T value ) { lua_pushnumber( L, static_cast<double>( value ) ); }
    static T Check( lua_State* L, int index ) { return static_cast<T>( luaL_checknumber( L, index ) ); }
};

// Luau has one number type; an integer is a number with nothing after the
// point that fits the C++ type. Beyond 2^53 a double cannot hold every
// integer, so 64-bit values are exact only up to there.
template <typename T>
    requires( std::integral<T> and not std::same_as<T, bool> )
struct LuaTraits<T>
{
    static constexpr const char* cName = "integer";
    static bool Is( lua_State* L, int index )
    {
        if ( lua_type( L, index ) != LUA_TNUMBER )
            return false;
        return Fits( lua_tonumber( L, index ) );
    }
    static void Push( lua_State* L, T value ) { lua_pushnumber( L, static_cast<double>( value ) ); }
    static T Check( lua_State* L, int index )
    {
        const double number = luaL_checknumber( L, index );
        if ( not Fits( number ) )
            luaL_argerror( L, index, "integer out of range or with a fraction" );
        return static_cast<T>( number );
    }

private:
    static bool Fits( double number )
    {
        // Both bounds are powers of two, so exact as doubles.
        const double low = static_cast<double>( std::numeric_limits<T>::min() );
        const double high = std::ldexp( 1.0, std::numeric_limits<T>::digits );
        return number == std::floor( number ) and number >= low and number < high;
    }
};

template <>
struct LuaTraits<string>
{
    static constexpr const char* cName = "string";
    static bool Is( lua_State* L, int index ) { return lua_type( L, index ) == LUA_TSTRING; }
    static void Push( lua_State* L, const string& value ) { lua_pushlstring( L, value.data(), value.size() ); }
    static string Check( lua_State* L, int index )
    {
        size_t length = 0;
        const char* text = luaL_checklstring( L, index, &length );
        return string( text, length );
    }
};

// Points into the Luau string: valid while that stays on the stack - for
// the arguments of a bound function, until it returns.
template <>
struct LuaTraits<string_view>
{
    static constexpr const char* cName = "string";
    static bool Is( lua_State* L, int index ) { return lua_type( L, index ) == LUA_TSTRING; }
    static void Push( lua_State* L, string_view value ) { lua_pushlstring( L, value.data(), value.size() ); }
    static string_view Check( lua_State* L, int index )
    {
        size_t length = 0;
        const char* text = luaL_checklstring( L, index, &length );
        return string_view( text, length );
    }
};

template <LuaVectorLike T>
struct LuaTraits<T>
{
    static constexpr const char* cName = "vector";
    static bool Is( lua_State* L, int index ) { return lua_isvector( L, index ); }
    static void Push( lua_State* L, const T& value ) { lua_pushvector( L, value.x, value.y, value.z ); }
    static T Check( lua_State* L, int index )
    {
        const float* v = luaL_checkvector( L, index );
        return T{ v[0], v[1], v[2] };
    }
};

// nil or nothing is an empty optional.
template <typename T>
struct LuaTraits<opt<T>>
{
    static constexpr const char* cName = LuaTraits<T>::cName;
    static bool Is( lua_State* L, int index ) { return lua_isnoneornil( L, index ) or LuaTraits<T>::Is( L, index ); }
    static void Push( lua_State* L, const opt<T>& value )
    {
        if ( value )
            LuaTraits<T>::Push( L, *value );
        else
            lua_pushnil( L );
    }
    static opt<T> Check( lua_State* L, int index )
    {
        if ( lua_isnoneornil( L, index ) )
            return std::nullopt;
        return LuaTraits<T>::Check( L, index );
    }
};

// Any value, held: a function to call later, a table to keep.
template <>
struct LuaTraits<LuaValue>
{
    static constexpr const char* cName = "value";
    static bool Is( lua_State*, int ) { return true; }
    static void Push( lua_State* L, const LuaValue& value ) { value.Push( L ); }
    static LuaValue Check( lua_State* L, int index ) { return LuaValue( L, index ); }
};

namespace detail
{
// A table, function or thread as a parameter: checked for its kind.
template <typename T, int Type>
struct LuaKindTraits
{
    static bool Is( lua_State* L, int index ) { return lua_type( L, index ) == Type; }
    static void Push( lua_State* L, const T& value ) { value.Push( L ); }
    static T Check( lua_State* L, int index )
    {
        if ( not Is( L, index ) )
            luaL_typeerror( L, index, lua_typename( L, Type ) );
        return T( LuaValue( L, index ) );
    }
};
}

template <>
struct LuaTraits<LuaTable> : detail::LuaKindTraits<LuaTable, LUA_TTABLE>
{
    static constexpr const char* cName = "table";
};

template <>
struct LuaTraits<LuaFunction> : detail::LuaKindTraits<LuaFunction, LUA_TFUNCTION>
{
    static constexpr const char* cName = "function";
};

template <>
struct LuaTraits<LuaThread> : detail::LuaKindTraits<LuaThread, LUA_TTHREAD>
{
    static constexpr const char* cName = "thread";
};

// Thrown by a bound function: a script error at the line that called it,
// "player.luau:12: what went wrong".
class LuaError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

// The arguments from here to the end, however many: the last parameter of
// a bound function that takes `...`. Passed to a call, they are spread out.
struct LuaRest
{
    vector<LuaValue> mValues;
};

// Returned by a bound function to suspend the coroutine that called it,
// handing `mValue` to whoever resumes it.
struct LuaYield
{
    LuaValue mValue;
};

template <typename T>
void LuaPush( lua_State* L, const T& value )
{
    LuaTraits<std::remove_cvref_t<T>>::Push( L, value );
}

inline void LuaPush( lua_State* L, const char* text )
{
    lua_pushstring( L, text );
}

inline void LuaPush( lua_State* L, const LuaRest& rest )
{
    for ( const LuaValue& value : rest.mValues )
        value.Push( L );
}

template <typename K>
void LuaPush( lua_State* L, const LuaTableEntry<K>& entry )
{
    entry.Value().Push( L );
}

template <typename T>
std::remove_cvref_t<T> LuaCheck( lua_State* L, int index )
{
    return LuaTraits<std::remove_cvref_t<T>>::Check( L, index );
}

namespace detail
{
// Makes `L` the thread LuaState works on and returns the one before:
// what lets C++ called from a coroutine use the coroutine's stack.
lua_State* SwapActiveThread( lua_State* L );

class ActiveThreadScope
{
public:
    explicit ActiveThreadScope( lua_State* L ) : mPrevious( SwapActiveThread( L ) ) {}
    ~ActiveThreadScope() { SwapActiveThread( mPrevious ); }
    ActiveThreadScope( const ActiveThreadScope& ) = delete;
    ActiveThreadScope& operator=( const ActiveThreadScope& ) = delete;

private:
    lua_State* mPrevious;
};

template <typename F>
struct Signature : Signature<decltype( &F::operator() )>
{
};

template <typename R, typename... A>
struct Signature<R ( * )( A... )>
{
    using Result = R;
    using Args = std::tuple<A...>;
};

template <typename R, typename... A>
struct Signature<R ( * )( A... ) noexcept> : Signature<R ( * )( A... )>
{
};

template <typename C, typename R, typename... A>
struct Signature<R ( C::* )( A... )> : Signature<R ( * )( A... )>
{
};

template <typename C, typename R, typename... A>
struct Signature<R ( C::* )( A... ) const> : Signature<R ( * )( A... )>
{
};

template <typename C, typename R, typename... A>
struct Signature<R ( C::* )( A... ) noexcept> : Signature<R ( * )( A... )>
{
};

template <typename C, typename R, typename... A>
struct Signature<R ( C::* )( A... ) const noexcept> : Signature<R ( * )( A... )>
{
};

// A lambda, functor or function pointer: what becomes a bound function
// when it is stored in a table.
template <typename T>
concept IsCallable = requires { &T::operator(); } or std::is_function_v<std::remove_pointer_t<T>>;

// A lua_State* parameter is handed the state and takes no argument.
template <typename A>
constexpr bool cIsState = std::is_same_v<std::remove_cvref_t<A>, lua_State*>;

// The stack slot of each parameter, arguments counted from `first`.
template <typename... A>
constexpr array<int, sizeof...( A )> StackSlots( int first )
{
    array<int, sizeof...( A )> slots{};
    int next = first;
    size_t i = 0;
    ( ( slots[i++] = cIsState<A> ? 0 : next++ ), ... );
    return slots;
}

template <typename A>
std::remove_cvref_t<A> Fetch( lua_State* L, int slot )
{
    if constexpr ( cIsState<A> )
        return L;
    else if constexpr ( std::is_same_v<std::remove_cvref_t<A>, LuaRest> )
    {
        LuaRest rest;
        for ( int index = slot; index <= lua_gettop( L ); ++index )
            rest.mValues.emplace_back( L, index );
        return rest;
    }
    else
        return LuaCheck<A>( L, slot );
}

template <typename T>
struct IsTuple : std::false_type
{
};

template <typename... T>
struct IsTuple<std::tuple<T...>> : std::true_type
{
};

// Pushes what a bound function returned; a tuple is several results, a
// LuaYield suspends the coroutine.
template <typename R>
int PushResults( lua_State* L, R&& result )
{
    if constexpr ( std::is_same_v<std::remove_cvref_t<R>, LuaYield> )
    {
        result.mValue.Push( L );
        return lua_yield( L, 1 );
    }
    else if constexpr ( IsTuple<std::remove_cvref_t<R>>::value )
    {
        std::apply( [L]( const auto&... values ) { ( LuaPush( L, values ), ... ); }, result );
        return static_cast<int>( std::tuple_size_v<std::remove_cvref_t<R>> );
    }
    else
    {
        LuaPush( L, result );
        return 1;
    }
}

template <typename Tuple>
struct DropFirst;

template <typename Head, typename... Tail>
struct DropFirst<std::tuple<Head, Tail...>>
{
    using Type = std::tuple<Tail...>;
};

// Calls `fn` with `prefix` (a method's object) and then the parameters `A`
// read from the stack from slot `first` on; pushes what it returns.
template <typename F, typename... A, typename... Prefix>
int Invoke( lua_State* L, F& fn, int first, std::type_identity<std::tuple<A...>>, Prefix&... prefix )
{
    const auto slots = StackSlots<A...>( first );
    return [&]<size_t... I>( std::index_sequence<I...> ) {
        // Braces read the arguments left to right.
        std::tuple<std::remove_cvref_t<A>...> args{ Fetch<A>( L, slots[I] )... };
        auto call = [&]( auto&&... rest ) -> decltype( auto ) {
            return fn( prefix..., std::forward<decltype( rest )>( rest )... );
        };
        if constexpr ( std::is_void_v<decltype( std::apply( call, std::move( args ) ) )> )
        {
            std::apply( call, std::move( args ) );
            return 0;
        }
        else
            return PushResults( L, std::apply( call, std::move( args ) ) );
    }( std::index_sequence_for<A...>{} );
}

template <typename F>
int CallBound( lua_State* L )
{
    F& fn = *static_cast<F*>( lua_touserdata( L, lua_upvalueindex( 1 ) ) );
    int results = 0;
    try
    {
        const ActiveThreadScope active( L );
        results = Invoke( L, fn, 1, std::type_identity<typename Signature<F>::Args>{} );
    }
    catch ( const LuaError& error )
    {
        // luaL_error puts the caller's file and line in front.
        luaL_error( L, "%s", error.what() );
    }
    return results;
}

template <typename T>
void Destroy( lua_State*, void* object )
{
    static_cast<T*>( object )->~T();
}

// Places `value` in a new userdata that destroys it when collected.
template <typename T>
T* NewOwned( lua_State* L, T value )
{
    static_assert( alignof( T ) <= 8, "userdata is 8-aligned" );
    static_assert( std::is_nothrow_move_constructible_v<T> );
    void* storage = lua_newuserdatadtor( L, sizeof( T ), &Destroy<T> );
    return new ( storage ) T( std::move( value ) );
}
}

// Pushes `fn` - a function or a lambda, captures and all - as a Luau
// function. Its parameters are read with LuaCheck: a wrong argument is a
// script error naming it; a last LuaRest takes the rest. A LuaError it
// throws is a script error at the calling line; any other exception, one
// with the exception's message. It returns nothing, one value, a tuple, or
// a LuaYield. While it runs, LuaState works on the thread that called it.
template <typename F>
void LuaPushFunction( lua_State* L, const char* name, F fn )
{
    detail::NewOwned( L, std::move( fn ) );
    lua_pushcclosure( L, &detail::CallBound<F>, name, 1 );
}

// Sets table[name] = fn for the table at `table`.
template <typename F>
void LuaSetFunction( lua_State* L, int table, const char* name, F fn )
{
    table = lua_absindex( L, table );
    LuaPushFunction( L, name, std::move( fn ) );
    lua_setfield( L, table, name );
}
}
