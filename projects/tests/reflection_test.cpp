// engine/reflection over types of its own: fields, properties, nested
// types, sequences and enums, reached by path and written to JSON.
#include "test.hpp"
#include "engine/reflection/reflection.hpp"
#include "engine/scene/components/light_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/editing/commands/field_command.hpp"
#include <nlohmann/json.hpp>

using namespace entt::literals;

namespace
{
enum class Shape
{
    Sphere,
    Box,
    WideBox
};

struct Point
{
    f32 mValue = 0.0f;
    vec2 mAt = vec2( 0 );
};

struct Thing
{
    string mName;
    f32 mRadius = 1.0f;
    Shape mShape = Shape::Sphere;
    vec3 mColor = vec3( 1 );
    quat mRotation = quat( 1, 0, 0, 0 );
    Point mOrigin;
    vector<Point> mPoints;
    i32 mLocked = 7;
    // Derived from mRadius; kept by the hook, never saved.
    f32 mArea = 0.0f;
    // Not described: counts the hook's calls.
    i32 mChanges = 0;
};

void ThingChanged( Thing& thing )
{
    thing.mArea = 4.0f * thing.mRadius * thing.mRadius;
    thing.mChanges++;
}

// Stands for a component over a runtime object: its value lives elsewhere
// and is reached through a getter and a setter.
struct Body
{
    Point mStored;
    f32 mStoredMass = 0.0f;
    i32 mSets = 0;
};

Point Spec( const Body& body ) { return body.mStored; }
void SetSpec( Body& body, Point spec ) { body.mStored = spec; body.mSets++; }
f32 Mass( const Body& body ) { return body.mStoredMass; }
void SetMass( Body& body, f32 mass ) { body.mStoredMass = mass < 0 ? 0 : mass; body.mSets++; }

void Describe()
{
    static const bool done = []
    {
        ReflectEnum<Shape>();
        TypeBuilder<Point>( "Point" )
            .Field<&Point::mValue>( "value" )
            .Field<&Point::mAt>( "at" );
        TypeBuilder<Thing>( "Thing" )
            .Field<&Thing::mName>( "name" )
            .Field<&Thing::mRadius>( "radius", { .mMin = 0.0f, .mMax = 10.0f } )
            .Field<&Thing::mShape>( "shape" )
            .Field<&Thing::mColor>( "color", { .mFlags = FieldInfo::Color } )
            .Field<&Thing::mRotation>( "rotation" )
            .Field<&Thing::mOrigin>( "origin" )
            .Field<&Thing::mPoints>( "points" )
            .Field<&Thing::mLocked>( "locked", { .mFlags = FieldInfo::ReadOnly } )
            .Field<&Thing::mArea>( "area", { .mFlags = FieldInfo::Transient | FieldInfo::ReadOnly } )
            .OnChanged<&ThingChanged>();
        TypeBuilder<Body>( "Body" )
            .Property<&SetSpec, &Spec>( "spec" )
            .Property<&SetMass, &Mass>( "mass" );
        return true;
    }();
    (void)done;
}

bool Throws( auto&& fn, string_view mentions = {} )
{
    try
    {
        fn();
    }
    catch ( const std::exception& e )
    {
        return mentions.empty() or string_view( e.what() ).find( mentions ) != string_view::npos;
    }
    return false;
}
}

TEST( Reflection_Json )
{
    Describe();
    Thing thing;
    thing.mName = "rock";
    thing.mRadius = 2.0f;
    thing.mShape = Shape::WideBox;
    thing.mRotation = glm::angleAxis( 1.0f, vec3( 0, 1, 0 ) );
    thing.mPoints = { { 1.0f, vec2( 1, 2 ) }, { 2.0f, vec2( 3, 4 ) } };
    thing.mArea = 16.0f;

    auto meta = Meta( thing );
    const json j = ToJson( meta );
    // Enums by name, snake_case; nested types as objects; derived state not
    // written; the hook's counter is not described at all.
    CHECK( j.at( "shape" ) == "wide_box" );
    CHECK( j.at( "points" ).size() == 2 and j.at( "points" )[1].at( "at" ) == json::array( { 3.0f, 4.0f } ) );
    CHECK( j.at( "origin" ).contains( "value" ) );
    CHECK( not j.contains( "area" ) and not j.contains( "changes" ) );

    Thing loaded;
    auto loadedMeta = Meta( loaded );
    FromJson( j, loadedMeta );
    CHECK( loaded.mName == "rock" and loaded.mRadius == 2.0f and loaded.mShape == Shape::WideBox );
    CHECK( loaded.mPoints.size() == 2 and loaded.mPoints[1].mAt == vec2( 3, 4 ) );
    CHECK( glm::all( glm::epsilonEqual( loaded.mRotation, thing.mRotation, 1e-6f ) ) );
    // The hook brought the derived state up to date, once.
    CHECK( loaded.mArea == 16.0f and loaded.mChanges == 1 );

    // An enum by number is read too; a name it does not have says which it has.
    CHECK( FromJson( json( 1 ), entt::resolve<Shape>() ).cast<Shape>() == Shape::Box );
    CHECK( Throws( [] { FromJson( json( "cube" ), entt::resolve<Shape>() ); }, "sphere, box, wide_box" ) );
    // A field of the wrong kind names itself.
    CHECK( Throws( [&] { FromJson( json{ { "radius", "big" } }, loadedMeta ); }, "Thing.radius" ) );
}

TEST( Reflection_Paths )
{
    Describe();
    Thing thing;
    thing.mPoints.resize( 3 );
    auto meta = Meta( thing );

    // Converted on the way: an int into a float, a name into an enum.
    SetField( meta, "radius", json( 3 ) );
    CHECK( thing.mRadius == 3.0f and thing.mArea == 36.0f );
    SetField( meta, "shape", json( "box" ) );
    CHECK( thing.mShape == Shape::Box );
    SetField( meta, "points[2].value", json( 5.5 ) );
    CHECK( thing.mPoints[2].mValue == 5.5f );
    SetField( meta, "origin.at", vec2( 7, 8 ) );
    CHECK( thing.mOrigin.mAt == vec2( 7, 8 ) );

    // Through fields a get is a reference into the object.
    GetField( meta, "points[0].at" ).cast<vec2&>() = vec2( 9 );
    CHECK( thing.mPoints[0].mAt == vec2( 9 ) );

    // Paths that lead nowhere say where they stopped.
    CHECK( Throws( [&] { SetField( meta, "raduis", json( 1 ) ); }, "has no field 'raduis'. Its fields: name, radius" ) );
    CHECK( Throws( [&] { SetField( meta, "points[3].value", json( 1 ) ); }, "has 3 elements" ) );
    CHECK( Throws( [&] { SetField( meta, "radius[0]", json( 1 ) ); }, "not a sequence" ) );
    CHECK( Throws( [&] { SetField( meta, "points[x]", json( 1 ) ); }, "not name[index]" ) );
    CHECK( Throws( [&] { SetField( meta, "origin..value", json( 1 ) ); }, "a name is missing" ) );
    CHECK( Throws( [&] { SetField( meta, "locked", json( 1 ) ); }, "read only" ) );
    CHECK( thing.mLocked == 7 );
}

TEST( Reflection_Properties )
{
    Describe();
    Body body;
    auto meta = Meta( body );

    // Into a value behind a getter: read, changed, set back - through the
    // setter, once.
    SetField( meta, "spec.value", json( 2.0 ) );
    CHECK( body.mStored.mValue == 2.0f and body.mSets == 1 );
    SetField( meta, "mass", json( -3 ) );
    CHECK( body.mStoredMass == 0.0f and body.mSets == 2 );

    // A get through a property is a copy: changing it changes nothing.
    entt::meta_any copy = GetField( meta, "spec.value" );
    copy.cast<f32&>() = 100.0f;
    CHECK( body.mStored.mValue == 2.0f );

    // And JSON of the whole reads the properties.
    const json j = ToJson( meta );
    CHECK( j.at( "spec" ).at( "value" ) == 2.0f and j.at( "mass" ) == 0.0f );
}

TEST( Reflection_Inspection )
{
    Describe();
    const entt::meta_type type = entt::resolve<Thing>();
    CHECK( TypeName( type ) == "Thing" );
    vector<string> names;
    for ( const auto [id, field] : type.data() )
        names.push_back( field.name() );
    // In the order described.
    CHECK( names.size() == 9 and names.front() == "name" and names.back() == "area" );
    const FieldInfo& radius = FieldInfoOf( type.data( "radius"_hs ) );
    CHECK( radius.mMax == 10.0f );
    CHECK( FieldInfoOf( type.data( "color"_hs ) ).Has( FieldInfo::Color ) );
    // A field described without a FieldInfo reads the default.
    CHECK( FieldInfoOf( entt::resolve<Point>().data( "value"_hs ) ).mFlags == FieldInfo::None );
}

// Components that describe themselves are saved from the description: names
// of fields, enums by name, nothing derived.
TEST( Reflection_ComponentsInLevelFiles )
{
    Fixture f;
    const Entity lamp = f.Create( EntityKind::Light );
    auto& light = f.scene.GetComponent<LightComponent>( lamp );
    light.mType = LightType::Point;
    light.mDistance = 13.0f;
    light.Update();

    const json saved = f.project.mLevel.ToJson( f.project );
    const json& pools = saved.at( "Scene" ).at( "Component pools" );
    const json& savedLight = pools.at( "light" ).at( std::to_string( lamp ) );
    CHECK( savedLight.at( "type" ) == "point" and savedLight.at( "distance" ) == 13.0f );
    CHECK( not savedLight.contains( "linear" ) and not savedLight.contains( "position" ) );
    CHECK( pools.at( "tag" ).at( std::to_string( lamp ) ).at( "name" ) == "Light" );
    CHECK( pools.at( "transform" ).at( std::to_string( lamp ) ).at( "position" ) == json::array( { 1.0f, 2.0f, 3.0f } ) );

    Level loaded;
    loaded.FromJson( saved, f.project );
    const auto& back = loaded.mScene.GetComponent<LightComponent>( lamp );
    // The attenuation is made again from the distance, not read.
    CHECK( back.mType == LightType::Point and back.mLinear == light.mLinear and back.mQuadratic == light.mQuadratic );
    CHECK( loaded.mScene.GetComponent<TransformComponent>( lamp ).mPosition == vec3( 1, 2, 3 ) );
}

// One undo step for any field of any reflected component, by path.
TEST( Reflection_SetFieldCommand )
{
    Fixture f;
    const Entity lamp = f.Create( EntityKind::Light );
    auto light = [&]() -> LightComponent& { return f.scene.GetComponent<LightComponent>( lamp ); };
    light().mType = LightType::Point;
    const f32 distance = light().mDistance;
    const f32 linear = light().mLinear;

    f.history.Execute( CreateScope<SetFieldCommand>( f.scene, lamp, LightComponent::ID(), "distance",
                                                     entt::meta_any( distance ), entt::meta_any( 13.0f ) ) );
    CHECK( f.history.NextUndoName() == "light.distance" );
    // Set through reflection, so OnChanged made the attenuation again.
    CHECK( light().mDistance == 13.0f and light().mLinear != linear );
    f.history.Undo();
    CHECK( light().mDistance == distance and light().mLinear == linear );
    f.history.Redo();
    CHECK( light().mDistance == 13.0f );

    // A component that is gone is left alone.
    f.scene.RemoveComponent<LightComponent>( lamp );
    f.history.Undo();
    CHECK( not f.scene.HasComponent<LightComponent>( lamp ) );
}
