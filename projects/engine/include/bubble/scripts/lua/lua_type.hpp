#pragma once
#include "bubble/scripts/lua/lua_stack.hpp"
#include "bubble/scripts/lua/lua_state.hpp"
#include "bubble/types/opt_ref.hpp"
#include <functional>
#include <stdexcept>
#include <typeindex>

namespace bubble
{
template <typename T>
class LuaTypeBuilder;

// An engine type as scripts see it: a userdata with a tag of its own, whose
// fields and methods are found by the name's atom - `light.brightness` and
// `body:apply_impulse( v )` cost an array lookup, not a string compare. A
// misspelt name is an error that lists what the type has. Built with
// LuaTypeBuilder; reflection will build them from a component's description.
// The userdata holds a T by value - for a component that T is a handle
// (entity and component), never an address in a pool.
class LuaType
{
public:
    const string& Name() const { return mName; }
    int Tag() const { return mTag; }

    // The object at `index`, or nothing when it is not of this type.
    template <typename T>
    OptRef<T> To( lua_State* L, int index ) const
    {
        Expect<T>();
        if ( void* data = lua_touserdatatagged( L, index, mTag ) )
            return *static_cast<T*>( data );
        return std::nullopt;
    }

    // The object at `index`, or a script error: "light expected, got number".
    template <typename T>
    T& Check( lua_State* L, int index ) const
    {
        Expect<T>();
        return *static_cast<T*>( CheckData( L, index ) );
    }

    // Pushes a new T of this type, built from `args`.
    template <typename T, typename... Args>
    T& PushNew( lua_State* L, Args&&... args ) const
    {
        Expect<T>();
        return *new ( lua_newuserdatataggedwithmetatable( L, sizeof( T ), mTag ) ) T( std::forward<Args>( args )... );
    }

    LuaType( LuaState& state, string name, int tag, std::type_index type );

private:
    template <typename T>
    friend class LuaTypeBuilder;

    // The userdata's storage is untyped here and typed again by the
    // builder's lambdas, which know T.
    using Getter = std::function<void( lua_State* L, void* data )>;
    using Setter = std::function<void( lua_State* L, void* data, int index )>;
    // Arguments from stack slot 2 on; returns how many results it pushed.
    using Method = std::function<int( lua_State* L, void* data )>;

    struct Member
    {
        string mName;
        Getter mGet;
        Setter mSet;
        Method mMethod;
    };

    static LuaType& Install( LuaState& state, string_view name, std::type_index type, lua_Destructor destroy );

    template <typename T>
    void Expect() const
    {
        if ( mType != std::type_index( typeid( T ) ) )
            throw std::logic_error( "Luau type " + mName + " holds another C++ type" );
    }

    void* CheckData( lua_State* L, int index ) const;
    void AddField( string_view name, Getter get, Setter set );
    void AddMethod( string_view name, Method method );
    Member& Add( string_view name );
    OptRef<const Member> Find( int atom, string_view name ) const;
    [[noreturn]] void NoMember( lua_State* L, string_view name, bool method ) const;

    static int Index( lua_State* L );
    static int NewIndex( lua_State* L );
    static int Namecall( lua_State* L );

    LuaState& mState;
    string mName;
    int mTag = 0;
    std::type_index mType;
    vector<Member> mMembers;
    // Member index by atom; -1 where an atom names nothing here.
    vector<i16> mByAtom;
    bool mMissingAtoms = false;
};

// Builds a LuaType for a C++ struct from its members and functions.
template <typename T>
class LuaTypeBuilder
{
public:
    // A new type whose userdata hold a T, destroyed when collected.
    LuaTypeBuilder( LuaState& state, string_view name )
        : mType( LuaType::Install( state, name, typeid( T ), &detail::Destroy<T> ) )
    {
    }

    template <typename M>
    LuaTypeBuilder& Field( string_view name, M T::* member )
    {
        mType.AddField(
            name, [member]( lua_State* L, void* data ) { LuaPush( L, static_cast<T*>( data )->*member ); },
            [member, &type = mType]( lua_State* L, void* data, int index ) {
                // The key being set is at slot 2.
                if ( not LuaTraits<M>::Is( L, index ) )
                    luaL_error( L, "%s.%s: %s expected, got %s", type.Name().c_str(), lua_tostring( L, 2 ),
                                LuaTraits<M>::cName, luaL_typename( L, index ) );
                static_cast<T*>( data )->*member = LuaCheck<M>( L, index );
            } );
        return *this;
    }

    template <typename M>
    LuaTypeBuilder& ReadOnly( string_view name, M T::* member )
    {
        mType.AddField(
            name, [member]( lua_State* L, void* data ) { LuaPush( L, static_cast<T*>( data )->*member ); }, {} );
        return *this;
    }

    // `fn( T& self, args... )`; arguments are checked like LuaPushFunction's.
    template <typename F>
    LuaTypeBuilder& Method( string_view name, F fn )
    {
        using Args = typename detail::DropFirst<typename detail::Signature<F>::Args>::Type;
        mType.AddMethod( name, [fn = std::move( fn )]( lua_State* L, void* data ) mutable {
            T& self = *static_cast<T*>( data );
            return detail::Invoke( L, fn, 2, std::type_identity<Args>{}, self );
        } );
        return *this;
    }

    LuaType& Type() { return mType; }

private:
    LuaType& mType;
};
}
