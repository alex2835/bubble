// Prefabs: a file of nodes with ids that last, instances made of it with
// links leading inside them, and a level that keeps an instance as its
// prefab and what differs from it - which loads back the same, and follows
// what the prefab becomes.
#include "bubble/scene/prefab.hpp"
#include "bubble/scene/scene_file.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
struct Light
{
    f32 mBrightness = 1;
    EntityPath mTarget;
};

struct Health
{
    i32 mValue = 100;
};

Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

AssetPath HousePath()
{
    return *AssetPath::TryFrom( "props/house.prefab" );
}

struct Fixture
{
    Reflection mReflection;
    ComponentTypes mComponentTypes{ mReflection };
    AssetCatalog mAssets;
    AssetId mHouseId = AssetId::Generate();
    Ref<const Prefab> mHouse;

    Fixture()
    {
        mAssets.Add( mHouseId, HousePath() );
        mReflection.Type<Light>( "light" )
            .Field<&Light::mBrightness>( "brightness" )
            .Field<&Light::mTarget>( "target" );
        mReflection.Type<Health>( "health" ).Field<&Health::mValue>( "value" );
        mComponentTypes.Add<Light>();
        mComponentTypes.Add<Health>();
        mHouse = CreateRef<const Prefab>( Parse( R"({
  "version": 1,
  "next_id": 3,
  "root": {
    "id": 0,
    "children": [
      { "id": 1, "name": "door", "components": { "light": { "target": "/window" }, "health": { "value": 50 } } },
      { "id": 2, "name": "window" }
    ]
  }
})" ) );
    }

    FileContext Files( PrefabSource prefabs = {} ) const
    {
        return FileContext( mComponentTypes, mAssets, std::move( prefabs ) );
    }

    Prefab Parse( string_view text ) const
    {
        auto prefabMaybe = Prefab::TryParse( text, Files() );
        REQUIRE_MESSAGE( prefabMaybe, prefabMaybe.error() );
        return std::move( *prefabMaybe );
    }

    string Why( string_view text ) const
    {
        const auto prefabMaybe = Prefab::TryParse( text, Files() );
        REQUIRE_FALSE( prefabMaybe );
        return prefabMaybe.error();
    }

    PrefabSource Source( Ref<const Prefab> prefab ) const
    {
        return [prefab, house = mHouseId]( const AssetId& prefabId ) -> expected<Ref<const Prefab>, string> {
            if ( prefabId == house )
                return prefab;
            return unexpected( "no such file"s );
        };
    }

    Entity Find( const Scene& scene, string_view path ) const
    {
        const auto foundMaybe = TryResolve( scene, EntityPath{ Make( path ) } );
        REQUIRE_MESSAGE( foundMaybe, foundMaybe.error() );
        return *foundMaybe;
    }

    string Save( const Scene& scene, Ref<const Prefab> prefab ) const
    {
        auto savedMaybe = TrySaveScene( scene, Files( Source( prefab ) ) );
        REQUIRE_MESSAGE( savedMaybe, savedMaybe.error() );
        return *savedMaybe;
    }

    Scene Load( string_view text, Ref<const Prefab> prefab ) const
    {
        auto sceneMaybe = TryLoadScene( text, Files( Source( prefab ) ) );
        REQUIRE_MESSAGE( sceneMaybe, sceneMaybe.error() );
        return std::move( *sceneMaybe );
    }
};
}

TEST_CASE( "A prefab made from a subtree keeps ids, leads its links from its root, and reads back as written" )
{
    Fixture f;
    Scene scene;
    const Entity house = CreateChild( scene, scene.Root(), Make( "house" ) );
    const Entity door = CreateChild( scene, house, Make( "door" ) );
    const Entity window = CreateChild( scene, house, Make( "window" ) );
    scene.Add<Light>( door ).mTarget = EntityPath{ Make( "/house/window" ) };
    scene.Add<Health>( window ).mValue = 7;

    hmap<Entity, u32> ids{ { window, 5 } };
    auto prefabMaybe = Prefab::TryFromScene( scene, house, f.mComponentTypes, ids, 1 );
    REQUIRE_MESSAGE( prefabMaybe, prefabMaybe.error() );
    const Prefab& prefab = *prefabMaybe;
    // The window keeps its 5; the door gets the next past every id there is.
    CHECK( ids[house] == 0 );
    CHECK( ids[window] == 5 );
    CHECK( ids[door] == 6 );
    CHECK( prefab.NextId() == 7 );
    REQUIRE( prefab.Nodes().size() == 3 );
    CHECK( prefab.Root().mChildren == vector<u32>{ 6, 5 } );
    CHECK( prefab.TryFind( 6 )->mName.View() == "door" );
    const Value& light = prefab.TryFind( 6 )->mComponents.front().second;
    CHECK( light.TryAs<Value::Record>()->at( 1 ).mValue == Value( EntityPath{ Make( "/window" ) } ) );

    const auto textMaybe = prefab.TryWrite( f.Files() );
    REQUIRE_MESSAGE( textMaybe, textMaybe.error() );
    const Prefab again = f.Parse( *textMaybe );
    CHECK( again.TryWrite( f.Files() ) == *textMaybe );
    CHECK( again.NextId() == 7 );
}

TEST_CASE( "A prefab file that is wrong says where" )
{
    Fixture f;
    CHECK( f.Why( R"({ "version": 1, "root": { "id": 0 } })" ) ==
           "no next_id: a prefab file says which id its next node gets" );
    CHECK( f.Why( R"({ "version": 1, "next_id": 2, "root": { "id": 0, "children": [ { "id": 2, "name": "a" } ] } })" )
               .ends_with( "next_id: 2 is not past every id: the highest is 2" ) );
    CHECK( f.Why( R"({ "version": 1, "next_id": 5, "root": { "id": 1 } })" ).ends_with( "/: the root's id is 0" ) );
    CHECK( f.Why( R"({ "version": 1, "next_id": 5, "root": { "id": 0, "children": [
               { "id": 1, "name": "a" }, { "id": 1, "name": "b" } ] } })" )
               .ends_with( "/b: a second node with the id 1" ) );
    CHECK( f.Why( R"({ "version": 1, "next_id": 5, "root": { "id": 0, "components": { "lamp": {} } } })" )
               .ends_with( "/: lamp is not a component (components: light, health)" ) );
    CHECK( f.Why( R"({ "version": 1, "next_id": 5, "root": { "id": 0, "name": "house" } })" )
               .ends_with( "/: the root has no name: an instance is named by its level" ) );
    CHECK( f.Why( R"({ "version": 1, "next_id": 5, "nodes": [], "root": { "id": 0 } })" )
               .ends_with( "nodes is not a key of a prefab (keys: version, asset, next_id, root)" ) );
}

TEST_CASE( "An instance is the prefab's nodes as entities, its links leading inside it" )
{
    Fixture f;
    Scene scene;
    const Entity house = Instantiate( scene, *f.mHouse, f.mHouseId, scene.Root(), Make( "house" ), f.mComponentTypes );
    const Entity door = f.Find( scene, "/house/door" );
    CHECK( scene.Get<PrefabInstance>( house ).mPrefab == f.mHouseId );
    CHECK( scene.Get<PrefabLink>( house ).mNode == 0 );
    CHECK( scene.Get<PrefabLink>( door ).mInstance == house );
    CHECK( scene.Get<PrefabLink>( door ).mNode == 1 );
    CHECK( scene.Get<Light>( door ).mTarget.mPath.View() == "/house/window" );
    CHECK( scene.Get<Health>( door ).mValue == 50 );
    // A field the file left out has its default.
    CHECK( scene.Get<Light>( door ).mBrightness == 1.0f );

    // A second one is named apart, and leads within itself.
    Instantiate( scene, *f.mHouse, f.mHouseId, scene.Root(), Make( "house" ), f.mComponentTypes );
    CHECK( scene.Get<Light>( f.Find( scene, "/house_2/door" ) ).mTarget.mPath.View() == "/house_2/window" );
}

TEST_CASE( "A level keeps an instance as its prefab and what differs, and loads it back the same" )
{
    Fixture f;
    Scene scene;
    const Entity house = Instantiate( scene, *f.mHouse, f.mHouseId, scene.Root(), Make( "house" ), f.mComponentTypes );
    const Entity door = f.Find( scene, "/house/door" );
    scene.Get<Health>( door ).mValue = 75;
    scene.Remove<Light>( door );
    scene.Add<Health>( f.Find( scene, "/house/window" ) ).mValue = 3;
    scene.Add<Health>( CreateChild( scene, house, Make( "sign" ) ) );

    const string text = f.Save( scene, f.mHouse );
    CHECK( text.find( R"("path": "props/house.prefab")" ) != string::npos );
    CHECK( text.find( R"("health.value": 75)" ) != string::npos );
    CHECK( text.find( R"("remove": ["1:light"])" ) != string::npos );
    CHECK( text.find( R"("sign")" ) != string::npos );
    // What follows the prefab is not in the level.
    CHECK( text.find( "window" ) == string::npos );

    const Scene loaded = f.Load( text, f.mHouse );
    const Entity loadedDoor = f.Find( loaded, "/house/door" );
    CHECK( loaded.Get<Health>( loadedDoor ).mValue == 75 );
    CHECK_FALSE( loaded.Has<Light>( loadedDoor ) );
    CHECK( loaded.Get<Health>( f.Find( loaded, "/house/window" ) ).mValue == 3 );
    CHECK( loaded.Has<Health>( f.Find( loaded, "/house/sign" ) ) );
    CHECK_FALSE( loaded.Has<PrefabLink>( f.Find( loaded, "/house/sign" ) ) );
    CHECK( f.Save( loaded, f.mHouse ) == text );
}

TEST_CASE( "An instance follows what its prefab becomes, and keeps what the level made differ" )
{
    Fixture f;
    Scene scene;
    Instantiate( scene, *f.mHouse, f.mHouseId, scene.Root(), Make( "house" ), f.mComponentTypes );
    scene.Get<Health>( f.Find( scene, "/house/door" ) ).mValue = 75;
    scene.Destroy( f.Find( scene, "/house/window" ) );
    const string text = f.Save( scene, f.mHouse );

    // The door brighter, a chimney new; the window, gone in the level, stays gone.
    const auto changed = CreateRef<const Prefab>( f.Parse( R"({
  "version": 1,
  "next_id": 4,
  "root": {
    "id": 0,
    "children": [
      { "id": 1, "name": "door", "components": { "light": { "brightness": 2 }, "health": { "value": 50 } } },
      { "id": 2, "name": "window" },
      { "id": 3, "name": "chimney" }
    ]
  }
})" ) );
    const Scene loaded = f.Load( text, changed );
    const Entity door = f.Find( loaded, "/house/door" );
    CHECK( loaded.Get<Light>( door ).mBrightness == 2.0f );
    CHECK( loaded.Get<Health>( door ).mValue == 75 );
    CHECK( f.Find( loaded, "/house/chimney" ) != Entity::Null );
    CHECK_FALSE( TryResolve( loaded, EntityPath{ Make( "/house/window" ) } ) );
}

TEST_CASE( "What a level says of a node its prefab no longer has is dropped, and the level loads" )
{
    Fixture f;
    Scene scene;
    Instantiate( scene, *f.mHouse, f.mHouseId, scene.Root(), Make( "house" ), f.mComponentTypes );
    scene.Get<Health>( f.Find( scene, "/house/door" ) ).mValue = 75;
    const string text = f.Save( scene, f.mHouse );

    const auto doorless =
        CreateRef<const Prefab>( f.Parse( R"({ "version": 1, "next_id": 3, "root": { "id": 0, "children": [
            { "id": 2, "name": "window" } ] } })" ) );
    const Scene loaded = f.Load( text, doorless );
    CHECK( ChildrenOf( loaded, f.Find( loaded, "/house" ) ).size() == 1 );
    // Saved again, the level no longer speaks of the door.
    CHECK( f.Save( loaded, doorless ).find( "\"1\"" ) == string::npos );
}

TEST_CASE( "An instance needs its prefab to be read or written, and a copy of one is plain entities" )
{
    Fixture f;
    Scene scene;
    const Entity house = Instantiate( scene, *f.mHouse, f.mHouseId, scene.Root(), Make( "house" ), f.mComponentTypes );
    const string text = f.Save( scene, f.mHouse );

    CHECK( TrySaveScene( scene, f.Files() ).error() ==
           "/house: an instance of props/house.prefab, and no prefabs to compare it with" );
    CHECK( TryLoadScene( text, f.Files() )
               .error()
               .ends_with( "/: an instance of props/house.prefab, and no prefabs to make it from" ) );
    const PrefabSource none = []( const AssetId& ) -> expected<Ref<const Prefab>, string> {
        return unexpected( "no such file"s );
    };
    CHECK( TryLoadScene( text, f.Files( none ) ).error().ends_with( "props/house.prefab: no such file" ) );

    const auto copyMaybe = TryEntityToJson( scene, house, f.Files() );
    REQUIRE( copyMaybe );
    const string copy = copyMaybe->Write();
    CHECK( copy.find( "prefab" ) == string::npos );
    CHECK( copy.find( "\"door\"" ) != string::npos );
}
