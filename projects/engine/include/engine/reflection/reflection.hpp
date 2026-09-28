#pragma once
#include "engine/types/json.hpp"
#include "engine/types/number.hpp"
#include "engine/types/string.hpp"
#include "engine/types/array.hpp"
#include "engine/utils/snake_case.hpp"
#include <magic_enum/magic_enum.hpp>
#include <entt/core/hashed_string.hpp>
#include <entt/meta/container.hpp>
#include <entt/meta/factory.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/resolve.hpp>

// What the engine knows about a type's fields at run time, so that one piece
// of code can save, load, show and edit any of them: a level file, the
// inspector, property.set and undo all go through here instead of through
// code written per component.
//
// The description is entt::meta's; this adds what the engine needs on top -
// FieldInfo for the editor, a hook for derived state, enums by name, JSON,
// and fields reached by a path of names.
//
// A type is described once, at startup:
//
//     TypeBuilder<LightComponent>( "Light" )
//         .Field<&LightComponent::mColor>( "color", { .mFlags = FieldInfo::Color } )
//         .Field<&LightComponent::mBrightness>( "brightness", { .mMin = 0, .mMax = 10 } )
//         .OnChanged<&LightChanged>();
//
// Names are snake_case: the same name is the key in a file, the step of a
// property path and, where there is one, the Lua field.
namespace bubble
{
struct Loader;

// What a value may need beyond itself to cross to or from JSON: the project's
// loader, for a resource a file names by path. Empty where there is no
// project at hand - a resource then cannot be read or written.
struct ReflectionContext
{
    Loader* mLoader = nullptr;
};

// What the editor needs to know about a field beyond its name and type.
struct FieldInfo
{
    enum Flags : u32
    {
        None = 0,
        // A vec3 or vec4 edited as a colour.
        Color = 1 << 0,
        // Shown but not edited.
        ReadOnly = 1 << 1,
        // Not shown.
        Hidden = 1 << 2,
        // Not written to files: state the owner rebuilds.
        Transient = 1 << 3,
        // A number edited with a slider over [mMin, mMax] rather than dragged.
        Slider = 1 << 4,
        // The slider's scale is logarithmic.
        Logarithmic = 1 << 5,
    };

    // A range for the widget; both 0 is unbounded.
    f32 mMin = 0.0f;
    f32 mMax = 0.0f;
    // Drag speed; 0 lets the widget choose.
    f32 mSpeed = 0.0f;
    u32 mFlags = None;
    const char* mTooltip = nullptr;
    // Whether to show the field, given its owner - a spot light's cone and
    // not a directional one's. Null is always.
    bool ( *mVisible )( const entt::meta_any& owner ) = nullptr;

    bool Has( Flags flag ) const { return ( mFlags & flag ) != 0; }
};

// What the editor shows about a type as a whole.
struct TypeInfo
{
    // A line under the inspector's title: what the fields do not say.
    const char* mNote = nullptr;
};

// The id under which a type keeps its OnChanged hook.
inline constexpr entt::id_type cOnChangedId = entt::hashed_string::value( "on_changed" );

// Describes T. Every Field and Property carries a FieldInfo, default or not.
template <typename T>
class TypeBuilder
{
public:
    explicit TypeBuilder( const char* name ) { entt::meta_factory<T>{}.type( name ); }

    // A data member, reached by reference: a path can go on into it, and a
    // set writes it in place.
    template <auto Member>
    TypeBuilder& Field( const char* name, FieldInfo info = {} )
    {
        entt::meta_factory<T>{}.template data<Member, entt::as_ref_t>( name ).template custom<FieldInfo>( info );
        return *this;
    }

    // A value behind a getter and a setter, T& first in both - free
    // functions or members. Reached by value: a path into it reads a copy,
    // changes it and sets it back.
    template <auto Setter, auto Getter>
    TypeBuilder& Property( const char* name, FieldInfo info = {} )
    {
        entt::meta_factory<T>{}.template data<Setter, Getter>( name ).template custom<FieldInfo>( info );
        return *this;
    }

    // A line the inspector shows under the type's title.
    TypeBuilder& Note( const char* note )
    {
        entt::meta_factory<T>{}.template custom<TypeInfo>( TypeInfo{ .mNote = note } );
        return *this;
    }

    // void( T& ), called after any of T's fields was set through this module
    // - by a path, from JSON - to bring derived state up to date.
    template <auto Hook>
    TypeBuilder& OnChanged()
    {
        entt::meta_factory<T>{}.template func<Hook>( cOnChangedId );
        return *this;
    }
};

namespace reflection_detail
{
// A name made at run time, kept for the life of the process: meta holds
// names by pointer.
const char* Intern( string name );
}

// An enum's values by name, snake_case: LightType::Directional is
// "directional" in a file and in a path.
template <typename E>
    requires std::is_enum_v<E>
void ReflectEnum()
{
    auto factory = entt::meta_factory<E>{}.type(
        reflection_detail::Intern( ToSnakeCase( magic_enum::enum_type_name<E>() ) ) );
    [&]<size_t... I>( std::index_sequence<I...> )
    {
        ( factory.template data<magic_enum::enum_values<E>()[I]>(
              reflection_detail::Intern( ToSnakeCase( magic_enum::enum_names<E>()[I] ) ) ),
          ... );
    }( std::make_index_sequence<magic_enum::enum_count<E>()>{} );
}

// The object behind a reference, as meta sees it. Changes through the
// result land in `object`.
template <typename T>
entt::meta_any Meta( T& object )
{
    return entt::forward_as_meta( object );
}

// The FieldInfo a field was described with.
const FieldInfo& FieldInfoOf( const entt::meta_data& field );
const TypeInfo& TypeInfoOf( const entt::meta_type& type );
// The type's name as described, or its C++ name if it was not.
string TypeName( const entt::meta_type& type );

/// Fields by path
// A path is names joined by dots, with an index after a name that is a
// sequence: "params.looping", "points[2].value". Both throw when the path
// leads nowhere, saying where it stopped.

// The value at `path`. A reference into `object` where the path runs
// through fields only; a copy once it passes a Property.
entt::meta_any GetField( entt::meta_any& object, string_view path );
// Sets the value at `path`, converting what can be converted (a number to
// an enum, an int to a float), writing copies back through every Property
// on the way, and calling OnChanged on each owner from the innermost out.
void SetField( entt::meta_any& object, string_view path, entt::meta_any value );
// The same with the value as JSON, as an operator gets it.
void SetField( entt::meta_any& object, string_view path, const json& value, const ReflectionContext& ctx = {} );

/// JSON
// Numbers, bools, strings, glm vectors and quat, enums by name, sequences
// as arrays and described types as objects of their fields - Transient
// ones left out. Anything else needs a codec.
json ToJson( const entt::meta_any& value, const ReflectionContext& ctx = {} );
entt::meta_any FromJson( const json& j, const entt::meta_type& type, const ReflectionContext& ctx = {} );
// Into an existing object: the fields `j` has are set, the rest keep their
// values; then OnChanged.
void FromJson( const json& j, entt::meta_any& object, const ReflectionContext& ctx = {} );

// How a type the rules above do not cover is written. Registered once.
using ToJsonFn = json ( * )( const entt::meta_any& value, const ReflectionContext& ctx );
using FromJsonFn = entt::meta_any ( * )( const json& j, const ReflectionContext& ctx );
void RegisterJsonCodec( const entt::meta_type& type, ToJsonFn to, FromJsonFn from );

// A type described with TypeBuilder: fields to walk, even if it has none.
bool IsDescribed( const entt::meta_type& type );

}
