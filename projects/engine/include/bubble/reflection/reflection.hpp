#pragma once
#include "bubble/core/name.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/function.hpp"
#include "bubble/types/handle.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"
#include <entt/meta/container.hpp>
#include <entt/meta/context.hpp>
#include <entt/meta/factory.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/resolve.hpp>

// What the engine knows about a type at run time, so that one piece of code
// can save, load, show and edit any of them: a level file, the inspector, a
// command and undo go through here instead of code written per component.
// The description is entt::meta's; this adds names, what the editor needs
// (FieldInfo), errors a person can read, and "this field changed".
//
// A type is described once, in the .cpp of its module:
//
//     reflection.Type<Light>( "light" )
//         .Field<&Light::mColor>( "color", { .mFlags = FieldInfo::Color } )
//         .Field<&Light::mBrightness>( "brightness", { .mMin = 0, .mMax = 10 } )
//         .Method<&Light::Blink>( "blink" );
//
// Names are snake_case Names: the same name is the key in a file, a step of
// a field path and the Luau field.
namespace bubble
{
using MetaAny = entt::meta_any;
using MetaType = entt::meta_type;
using MetaField = entt::meta_data;
using MetaMethod = entt::meta_func;

// What the editor needs to know about a field beyond its name and type.
struct FieldInfo
{
    enum Flags : u32
    {
        None = 0,
        // A vec3 or vec4 edited as a color.
        Color = 1 << 0,
        // Shown but not edited.
        ReadOnly = 1 << 1,
        // Not shown.
        Hidden = 1 << 2,
        // Not written to files: state the owner rebuilds.
        Transient = 1 << 3,
        // A number edited with a slider over [mMin, mMax] rather than dragged.
        Slider = 1 << 4,
        // Radians, edited as degrees; mMin and mMax are in degrees.
        Angle = 1 << 5,
    };

    // A range for the widget; both 0 is unbounded.
    f32 mMin = 0;
    f32 mMax = 0;
    u32 mFlags = None;
    const char* mTooltip = nullptr;
    // Whether to show the field, given its owner - a spot light's cone and
    // not a directional one's. Null is always.
    bool ( *mVisible )( const MetaAny& owner ) = nullptr;

    bool Has( Flags flag ) const { return ( mFlags & flag ) != 0; }
};

// What the editor and the console show about a method.
struct MethodInfo
{
    const char* mTooltip = nullptr;
};

// What the editor shows about a type as a whole.
struct TypeInfo
{
    // A line under the inspector's title: what the fields do not say.
    const char* mNote = nullptr;
};

using FieldListenerHandle = Handle<struct FieldListenerTag>;

namespace detail
{
// A name a description gives, declared: a bad one is a logic_error.
Name DeclareName( string_view name );
}

// Describes T, a step at a time. A bad name is a bug in the description: a
// logic_error, at startup.
template <typename T>
class TypeBuilder
{
public:
    TypeBuilder( entt::meta_ctx& context, Name name ) : mFactory( context ) { mFactory.type( name.Hash() ); }

    // A data member, reached by reference: a path goes on into it, and a set
    // writes it in place.
    template <auto Member>
    TypeBuilder& Field( string_view name, FieldInfo info = {} )
    {
        mFactory.template data<Member, entt::as_ref_t>( Declare( name ) ).template custom<FieldInfo>( info );
        return *this;
    }

    // A value behind a setter and a getter - members, or free functions
    // taking T& first. Reached by value: what a path reads is a copy.
    template <auto Setter, auto Getter>
    TypeBuilder& Property( string_view name, FieldInfo info = {} )
    {
        mFactory.template data<Setter, Getter>( Declare( name ) ).template custom<FieldInfo>( info );
        return *this;
    }

    // Something to do to a T: Luau, the operators and the console call it.
    template <auto Function>
    TypeBuilder& Method( string_view name, MethodInfo info = {} )
    {
        mFactory.template func<Function>( Declare( name ) ).template custom<MethodInfo>( info );
        return *this;
    }

    TypeBuilder& Note( const char* note )
    {
        mFactory.template custom<TypeInfo>( TypeInfo{ note } );
        return *this;
    }

private:
    static u32 Declare( string_view name ) { return detail::DeclareName( name ).Hash(); }

    entt::meta_factory<T> mFactory;
};

// The descriptions of types, and who listens for their fields. Passed
// around, never global: every test gets a clean one.
class Reflection
{
public:
    Reflection();
    ~Reflection();
    Reflection( const Reflection& ) = delete;
    Reflection& operator=( const Reflection& ) = delete;

    template <typename T>
    TypeBuilder<T> Type( string_view name )
    {
        const Name typeName = detail::DeclareName( name );
        mDescribed.insert( typeName );
        return TypeBuilder<T>( mContext, typeName );
    }

    template <typename T>
    MetaType TypeOf() const
    {
        return entt::resolve<T>( mContext );
    }

    // The object as reflection sees it, by reference: a set through it
    // changes `object`.
    template <typename T>
    MetaAny Ref( T& object ) const
    {
        return entt::forward_as_meta( mContext, object );
    }

    opt<MetaType> Find( Name type ) const;
    // For messages: a described type's name, what a person calls a number,
    // a bool or a text, the C++ name for anything else.
    string NameOf( const MetaType& type ) const;

    // The fields in the order they were described.
    void ForEachField( const MetaType& type, const function<void( Name, const MetaField& )>& fn ) const;
    static const FieldInfo& InfoOf( const MetaField& field );
    static const TypeInfo& InfoOf( const MetaType& type );

    // One field of `object` by name: a reference into it for a Field, a copy
    // for a Property.
    expected<MetaAny, string> Get( const MetaAny& object, Name field ) const;
    // Sets one field, converting what converts (an integer to a float), then
    // tells the field's listeners.
    expected<void, string> Set( MetaAny& object, Name field, MetaAny value );

    expected<MetaAny, string> Call( MetaAny& object, Name method, span<MetaAny> args ) const;

    // "This field of this type changed": a module fixes exactly what moved,
    // not everything about the type. Listeners are called with the owner.
    using FieldListener = function<void( MetaAny& owner )>;
    FieldListenerHandle OnFieldChanged( const MetaType& type, Name field, FieldListener listener );
    void RemoveListener( FieldListenerHandle listener );
    // For code that changed a field itself rather than through Set.
    void NotifyChanged( MetaAny& owner, Name field );

private:
    struct Listener
    {
        u32 mType = 0;
        Name mField;
        FieldListener mFn;
    };

    string FieldNames( const MetaType& type ) const;
    string MethodNames( const MetaType& type ) const;

    entt::meta_ctx mContext;
    hset<Name> mDescribed;
    SlotMap<Listener, FieldListenerTag> mListeners;
};
}
