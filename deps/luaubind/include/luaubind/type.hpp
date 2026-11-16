#pragma once
#include "luaubind/common.hpp"
#include "luaubind/stack.hpp"
#include "luaubind/state.hpp"
#include <concepts>
#include <functional>
#include <stdexcept>
#include <typeindex>

namespace luaubind
{
template <typename T>
class LuaTypeBuilder;

// An engine type as scripts see it: a userdata with a tag of its own, whose
// fields and methods are found by the name's atom - `light.brightness` and
// `body:apply_impulse( v )` cost an array lookup, not a string compare. A
// misspelt name is an error that lists what the type has. Built with
// LuaTypeBuilder; reflection will build them from a component's description.
// The userdata holds a T by value - for a component that T is a handle
// (entity and component), never an address in a pool. Two values of a T
// with == are equal in Luau when they are equal in C++; tostring gives what
// ToString says, or the type's name.
//
// Fields and methods run on the thread that reached them, as bound
// functions do, and a LuaError they throw is a script error at that line.
class LuaType
{
public:
    const string& Name() const { return mName; }
    int Tag() const { return mTag; }

    // The object at `index`, or null when it is not of this type.
    template <typename T>
    T* To( lua_State* L, int index ) const
    {
        Expect<T>();
        return static_cast<T*>( lua_touserdatatagged( L, index, mTag ) );
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

    // A new T of this type as a value C++ holds: what a field computed in
    // C++ returns when its C++ type has several Luau types - a component
    // handle, one type a component.
    template <typename T>
    LuaValue New( T value ) const
    {
        lua_State* L = mState.Active();
        PushNew<T>( L, std::move( value ) );
        return mState.Take();
    }

    // The T a value holds, when it is one of this type.
    template <typename T>
    T* TryTo( const LuaValue& value ) const
    {
        Expect<T>();
        if ( value.IsNil() )
            return nullptr;
        lua_State* L = mState.Active();
        value.Push( L );
        T* object = static_cast<T*>( lua_touserdatatagged( L, -1, mTag ) );
        lua_pop( L, 1 );
        return object;
    }

    // For the data codec: whether values of the type can be saved, the
    // bytes one saves as, and the value pushed again from them.
    bool Saves() const { return mEncode != nullptr; }
    string EncodeData( const void* data ) const { return mEncode( data ); }
    expected<void, string> TryDecodeData( lua_State* L, string_view bytes ) const { return mDecode( L, bytes ); }

    using Equals = bool ( * )( const void* a, const void* b );
    LuaType( LuaState& state, string name, int tag, std::type_index type, Equals equals );

private:
    template <typename T>
    friend class LuaTypeBuilder;

    // The userdata's storage is untyped here and typed again by the
    // builder's lambdas, which know T.
    using Getter = std::function<void( lua_State* L, void* data )>;
    using Setter = std::function<void( lua_State* L, void* data, int index )>;
    // Arguments from stack slot 2 on; returns how many results it pushed.
    using Method = std::function<int( lua_State* L, void* data )>;
    using Describe = std::function<string( const void* data )>;
    using Encode = std::function<string( const void* data )>;
    using Decode = std::function<expected<void, string>( lua_State* L, string_view bytes )>;

    struct Member
    {
        string mName;
        Getter mGet;
        Setter mSet;
        Method mMethod;
    };

    static LuaType& Install( LuaState& state,
                             string_view name,
                             std::type_index type,
                             lua_Destructor destroy,
                             Equals equals );
    void SetUser( std::type_index type );

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
    const Member* TryFind( int atom, string_view name ) const;
    [[noreturn]] void NoMember( lua_State* L, string_view name, bool method ) const;

    static int Index( lua_State* L );
    static int NewIndex( lua_State* L );
    static int Namecall( lua_State* L );
    static int Eq( lua_State* L );
    static int ToString( lua_State* L );

    LuaState& mState;
    string mName;
    int mTag = 0;
    std::type_index mType;
    Equals mEquals = nullptr;
    Describe mDescribe;
    Encode mEncode;
    Decode mDecode;
    vector<Member> mMembers;
    // Member index by atom; -1 where an atom names nothing here.
    vector<i16> mByAtom;
    bool mMissingAtoms = false;
};

namespace detail
{
template <typename T>
LuaType::Equals EqualsOf()
{
    if constexpr ( std::equality_comparable<T> )
        return
            []( const void* a, const void* b ) { return *static_cast<const T*>( a ) == *static_cast<const T*>( b ); };
    else
        return nullptr;
}

template <typename T>
const LuaType& UserTypeOf( lua_State* L )
{
    const LuaType* type = LuaState::Of( L ).TryFindType( std::type_index( typeid( T ) ) );
    if ( type == nullptr )
        throw std::logic_error( string( "Luau type " ) + LuaUserType<T>::cName + " crosses before it is registered" );
    return *type;
}

template <typename T>
bool IsUser( lua_State* L, int index )
{
    return UserTypeOf<T>( L ).template To<T>( L, index ) != nullptr;
}

template <typename T>
void PushUser( lua_State* L, const T& value )
{
    UserTypeOf<T>( L ).template PushNew<T>( L, value );
}

template <typename T>
T& CheckUser( lua_State* L, int index )
{
    return UserTypeOf<T>( L ).template Check<T>( L, index );
}
}

// Builds a LuaType for a C++ struct from its members and functions.
template <typename T>
class LuaTypeBuilder
{
public:
    // A new type whose userdata hold a T, destroyed when collected. A
    // LuaUserType is registered as the one its values cross as; twice is a
    // logic_error.
    LuaTypeBuilder( LuaState& state, string_view name )
        : mType( LuaType::Install( state, name, typeid( T ), &detail::Destroy<T>, detail::EqualsOf<T>() ) )
    {
        if constexpr ( LuaUserTypeLike<T> )
            mType.SetUser( typeid( T ) );
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
        mType.AddField( name, [member]( lua_State* L, void* data ) { LuaPush( L, static_cast<T*>( data )->*member ); },
                        {} );
        return *this;
    }

    // A field computed in C++: `get( T& self )` returns anything LuaTraits
    // knows, a LuaValue too; `set( T& self, value )`, when there is one,
    // takes the value written, checked as an argument is.
    template <typename Get>
    LuaTypeBuilder& Property( string_view name, Get get )
    {
        mType.AddField( name,
                        [get = std::move( get )]( lua_State* L, void* data ) mutable {
                            LuaPush( L, get( *static_cast<T*>( data ) ) );
                        },
                        {} );
        return *this;
    }

    template <typename Get, typename Set>
    LuaTypeBuilder& Property( string_view name, Get get, Set set )
    {
        using Value = std::remove_cvref_t<std::tuple_element_t<1, typename detail::Signature<Set>::Args>>;
        mType.AddField(
            name,
            [get = std::move( get )]( lua_State* L, void* data ) mutable {
                LuaPush( L, get( *static_cast<T*>( data ) ) );
            },
            [set = std::move( set ), &type = mType]( lua_State* L, void* data, int index ) mutable {
                if ( not LuaTraits<Value>::Is( L, index ) )
                    luaL_error( L, "%s.%s: %s expected, got %s", type.Name().c_str(), lua_tostring( L, 2 ),
                                LuaTraits<Value>::cName, luaL_typename( L, index ) );
                set( *static_cast<T*>( data ), LuaCheck<Value>( L, index ) );
            } );
        return *this;
    }

    // Values of the type can be saved - in a slot, in `progress`, as a
    // prop: `encode( const T& )` gives the bytes, `decode( bytes )` the T
    // again, or why it cannot.
    template <typename Encode, typename Decode>
    LuaTypeBuilder& Saved( Encode encode, Decode decode )
    {
        mType.mEncode = [encode = std::move( encode )]( const void* data ) {
            return string( encode( *static_cast<const T*>( data ) ) );
        };
        mType.mDecode = [decode = std::move( decode ), &type = mType]( lua_State* L,
                                                                       string_view bytes ) -> expected<void, string> {
            auto valueMaybe = decode( bytes );
            if ( not valueMaybe )
                return std::unexpected( std::move( valueMaybe.error() ) );
            type.template PushNew<T>( L, std::move( *valueMaybe ) );
            return {};
        };
        return *this;
    }

    // What tostring gives for a value: "/door" rather than "entity".
    template <typename F>
    LuaTypeBuilder& ToString( F describe )
    {
        mType.mDescribe = [describe = std::move( describe )]( const void* data ) {
            return string( describe( *static_cast<const T*>( data ) ) );
        };
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
