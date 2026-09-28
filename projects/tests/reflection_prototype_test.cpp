// A prototype, not the engine's reflection yet: LightComponent and
// RigidBodyComponent described once through entt::meta, and the three things
// that description has to be enough for - a field set by name (what an
// operator does), JSON both ways (what a level file does) and a walk over the
// fields with what an inspector needs to draw them.
#include "test.hpp"
#include "engine/scene/components/light_component.hpp"
#include "engine/scene/components/rigid_body_component.hpp"
#include <entt/core/hashed_string.hpp>
#include <entt/meta/factory.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/resolve.hpp>
#include <nlohmann/json.hpp>

using namespace entt::literals;

namespace
{
/// What the description says beyond name and type

// For the inspector. Plain data: meta keeps a copy per field.
struct FieldInfo
{
    f32 mMin = 0.0f;
    f32 mMax = 0.0f;
    f32 mSpeed = 1.0f;
    bool mColor = false;
    // Whether to show the field, given the whole component.
    bool ( *mVisible )( const entt::meta_any& component ) = nullptr;
};

template <typename Component>
bool TypeIsNot( const entt::meta_any& any, LightType type )
{
    return any.cast<const Component&>().mType != type;
}


/// Light: plain fields, derived state kept by a hook

void LightChanged( LightComponent& light )
{
    light.Update(); // attenuation from distance
}

bool NotDirectional( const entt::meta_any& c ) { return TypeIsNot<LightComponent>( c, LightType::Directional ); }
bool IsSpot( const entt::meta_any& c ) { return c.cast<const LightComponent&>().mType == LightType::Spot; }

void ReflectLight()
{
    entt::meta_factory<LightComponent>()
        .type( "Light" )
        .data<&LightComponent::mType>( "type" )
        .data<&LightComponent::mColor>( "color" )
            .custom<FieldInfo>( FieldInfo{ .mColor = true } )
        .data<&LightComponent::mBrightness>( "brightness" )
            .custom<FieldInfo>( FieldInfo{ .mMin = 0.0f, .mMax = 10.0f, .mSpeed = 0.01f } )
        .data<&LightComponent::mDistance>( "distance" )
            .custom<FieldInfo>( FieldInfo{ .mMin = 0.1f, .mMax = 3250.0f, .mVisible = NotDirectional } )
        .data<&LightComponent::mCutOff>( "cut_off" )
            .custom<FieldInfo>( FieldInfo{ .mMin = 0.0f, .mMax = 90.0f, .mVisible = IsSpot } )
        .data<&LightComponent::mOuterCutOff>( "outer_cut_off" )
            .custom<FieldInfo>( FieldInfo{ .mMin = 0.0f, .mMax = 90.0f, .mVisible = IsSpot } )
        .func<&LightChanged>( "changed" );
}


/// RigidBody: no fields of its own, properties over the Bullet body

f32 Mass( const RigidBodyComponent& c ) { return const_cast<RigidBody&>( c.mRigidBody ).GetMass(); }
void SetMass( RigidBodyComponent& c, f32 mass ) { c.mRigidBody.SetMass( c.mRigidBody.IsKinematic() ? 0.0f : mass ); }
f32 Friction( const RigidBodyComponent& c ) { return const_cast<RigidBody&>( c.mRigidBody ).GetFriction(); }
void SetFriction( RigidBodyComponent& c, f32 friction ) { c.mRigidBody.SetFriction( friction ); }
bool Kinematic( const RigidBodyComponent& c ) { return c.mRigidBody.IsKinematic(); }
void SetKinematic( RigidBodyComponent& c, bool kinematic )
{
    // Bullet treats only a massless body as kinematic.
    if ( kinematic )
        c.mRigidBody.SetMass( 0.0f );
    c.mRigidBody.SetKinematic( kinematic );
}

void ReflectRigidBody()
{
    entt::meta_factory<RigidBodyComponent>()
        .type( "RigidBody" )
        .data<&SetMass, &Mass>( "mass" )
            .custom<FieldInfo>( FieldInfo{ .mMin = 0.0f, .mMax = 1000.0f } )
        .data<&SetFriction, &Friction>( "friction" )
            .custom<FieldInfo>( FieldInfo{ .mMin = 0.0f, .mMax = 1.0f, .mSpeed = 0.01f } )
        .data<&SetKinematic, &Kinematic>( "kinematic" );
}

void Reflect()
{
    static const bool done = ( ReflectLight(), ReflectRigidBody(), true );
    (void)done;
}


/// What is written once, for every type

json ValueToJson( const entt::meta_any& value )
{
    if ( auto* v = value.try_cast<f32>() ) return *v;
    if ( auto* v = value.try_cast<bool>() ) return *v;
    if ( auto* v = value.try_cast<vec3>() ) return json{ v->x, v->y, v->z };
    if ( value.type().is_enum() ) return value.allow_cast<i32>().cast<i32>();
    throw std::runtime_error( std::format( "no JSON for {}", value.type().info().name() ) );
}

entt::meta_any ValueFromJson( const json& j, const entt::meta_type& type )
{
    if ( type == entt::resolve<f32>() ) return j.get<f32>();
    if ( type == entt::resolve<bool>() ) return j.get<bool>();
    if ( type == entt::resolve<vec3>() ) return vec3( j[0], j[1], j[2] );
    if ( type.is_enum() )
    {
        entt::meta_any number = j.get<i32>();
        if ( not number.allow_cast( type ) )
            throw std::runtime_error( std::format( "{} is not a {}", j.dump(), type.name() ) );
        return number;
    }
    throw std::runtime_error( std::format( "no value of {} from JSON", type.info().name() ) );
}

// After a field was set: whatever the type derives from its fields.
void Changed( entt::meta_any& component )
{
    if ( const auto changed = component.type().func( "changed"_hs ) )
        changed.invoke( component );
}

// What an operator would call: property.set{ entity, "Light.brightness", 2.5 }.
void SetField( entt::meta_any component, const char* field, const json& value )
{
    const entt::meta_type type = component.type();
    const entt::meta_data data = type.data( entt::hashed_string{ field } );
    if ( not data )
        throw std::runtime_error( std::format( "{} has no field '{}'", type.name(), field ) );
    if ( not data.set( component, ValueFromJson( value, data.type() ) ) )
        throw std::runtime_error( std::format( "{}.{}: cannot be set", type.name(), field ) );
    Changed( component );
}

json ToJson( entt::meta_any component )
{
    json j = json::object();
    for ( const auto [id, data] : component.type().data() )
        j[data.name()] = ValueToJson( data.get( component ) );
    return j;
}

void FromJson( entt::meta_any component, const json& j )
{
    for ( const auto [id, data] : component.type().data() )
        if ( j.contains( data.name() ) )
            data.set( component, ValueFromJson( j[data.name()], data.type() ) );
    Changed( component );
}

// What an inspector would draw, in order: "name [min, max]".
vector<string> Inspect( entt::meta_any component )
{
    vector<string> lines;
    for ( const auto [id, data] : component.type().data() )
    {
        const FieldInfo* info = data.custom();
        if ( info and info->mVisible and not info->mVisible( component ) )
            continue;
        lines.push_back( info ? std::format( "{} [{}, {}]", data.name(), info->mMin, info->mMax ) : string( data.name() ) );
    }
    return lines;
}

// The component by reference, as meta sees it.
template <typename Component>
entt::meta_any Meta( Component& component ) { return entt::forward_as_meta( component ); }
}

TEST( ReflectionPrototype_Light )
{
    Reflect();
    LightComponent light = LightComponent::CreatePointLight();

    // By name, and the derived attenuation follows.
    const f32 linearBefore = light.mLinear;
    SetField( Meta( light ), "distance", 7.0f );
    CHECK( light.mDistance == 7.0f and light.mLinear != linearBefore );
    SetField( Meta( light ), "type", (i32)LightType::Spot );
    CHECK( light.mType == LightType::Spot );

    // The file holds what is described and nothing derived.
    const json j = ToJson( Meta( light ) );
    CHECK( j.size() == 6 and j.contains( "cut_off" ) and not j.contains( "linear" ) and not j.contains( "position" ) );
    LightComponent loaded;
    FromJson( Meta( loaded ), j );
    CHECK( loaded.mType == LightType::Spot and loaded.mDistance == 7.0f and loaded.mLinear == light.mLinear );

    // A directional light has no distance or cones to show.
    SetField( Meta( light ), "type", (i32)LightType::Directional );
    CHECK( Inspect( Meta( light ) ).size() == 3 and Inspect( Meta( light ) )[2] == "brightness [0, 10]" );

    bool threw = false;
    try { SetField( Meta( light ), "brigthness", 1.0f ); } catch ( const std::exception& ) { threw = true; }
    CHECK( threw );
}

TEST( ReflectionPrototype_RigidBody )
{
    Reflect();
    RigidBodyComponent body;

    // Setters with side effects: a kinematic body loses its mass, and keeps
    // it at 0.
    SetField( Meta( body ), "mass", 5.0f );
    CHECK( body.mRigidBody.GetMass() == 5.0f );
    SetField( Meta( body ), "kinematic", true );
    CHECK( body.mRigidBody.IsKinematic() and body.mRigidBody.GetMass() == 0.0f );
    SetField( Meta( body ), "mass", 3.0f );
    CHECK( body.mRigidBody.GetMass() == 0.0f );

    const json j = ToJson( Meta( body ) );
    CHECK( j.at( "kinematic" ) == true and j.at( "mass" ) == 0.0f );
    RigidBodyComponent loaded;
    FromJson( Meta( loaded ), json{ { "mass", 2.0f }, { "friction", 0.25f } } );
    CHECK( loaded.mRigidBody.GetMass() == 2.0f and loaded.mRigidBody.GetFriction() == 0.25f );
}
