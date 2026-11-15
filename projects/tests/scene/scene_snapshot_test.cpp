// SceneSnapshot: a scene's data copied whole in memory and put back - the
// same entities under the same numbers, the components in the order they
// lay in, the next entity made the one the scene would have made.
#include "bubble/scene/component_types.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/scene_file.hpp"
#include "bubble/scene/scene_snapshot.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
struct Lantern
{
    f32 mGlow = 1;
};

// A world's runtime: never described, built again from the data.
struct Flame
{
    f32 mHeight = 0;
};

struct Heard final : ComponentListener
{
    vector<Entity> mAdded;
    vector<Entity> mRemoved;

    void Added( Entity entity ) override { mAdded.push_back( entity ); }
    void Replaced( Entity ) override {}
    void Removed( Entity entity ) override { mRemoved.push_back( entity ); }
};

Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

struct Fixture
{
    Reflection mReflection;
    ComponentTypes mComponentTypes{ mReflection };
    Scene mScene;
    Entity mHall;
    Entity mAttic;
    Entity mCellar;

    Fixture()
    {
        mReflection.Type<Lantern>( "lantern" ).Field<&Lantern::mGlow>( "glow" );
        mComponentTypes.Add<Lantern>();
        mHall = CreateChild( mScene, mScene.Root(), Make( "hall" ) );
        mAttic = CreateChild( mScene, mScene.Root(), Make( "attic" ) );
        mCellar = CreateChild( mScene, mScene.Root(), Make( "cellar" ) );
        // Out of the tree's order: the order they lie in is the scene's own.
        mScene.Add<Lantern>( mCellar, 3.0f );
        mScene.Add<Lantern>( mHall, 1.0f );
        mScene.Add<Lantern>( mAttic, 2.0f );
        // A number free, to come back at a newer version.
        mScene.Destroy( CreateChild( mScene, mScene.Root(), Make( "gone" ) ) );
    }
};
}

TEST_CASE( "A snapshot puts a scene back as it was: its entities under their numbers, its components in their order" )
{
    Fixture f;
    Scene& scene = f.mScene;
    const string saved = *TrySaveScene( scene, f.mComponentTypes );
    const vector<Entity> order = scene.EntitiesWith<Lantern>();
    const SceneSnapshot snapshot( scene, f.mComponentTypes );

    // What the scene makes next: the free number first, then a new one.
    const Entity next = scene.Create();
    const Entity after = scene.Create();
    // And a good deal else.
    scene.Destroy( f.mHall );
    Rename( scene, f.mAttic, Make( "loft" ) );
    scene.Get<Lantern>( f.mCellar ).mGlow = 9;
    scene.Add<Lantern>( scene.Root() );
    CreateChild( scene, scene.Root(), Make( "porch" ) );

    Heard heard;
    scene.Listen<Lantern>( heard );
    snapshot.Restore( scene, f.mComponentTypes );
    CHECK( *TrySaveScene( scene, f.mComponentTypes ) == saved );
    CHECK( scene.EntitiesWith<Lantern>() == order );
    CHECK( *TryResolve( scene, EntityPath{ Make( "/hall" ) } ) == f.mHall );
    CHECK( scene.Create() == next );
    CHECK( scene.Create() == after );

    // Every lantern there was heard going, every one in the snapshot coming.
    CHECK( heard.mRemoved.size() == 3 );
    CHECK( heard.mAdded == order );
}

TEST_CASE( "A snapshot keeps no runtime components, but the order every type's components lay in" )
{
    Fixture f;
    Scene& scene = f.mScene;
    for ( const Entity entity : { f.mCellar, f.mHall, f.mAttic } )
        scene.Add<Flame>( entity );
    // Built anew: to the end of its type.
    scene.Remove<Flame>( f.mHall );
    scene.Add<Flame>( f.mHall );
    const vector<Entity> flames = scene.EntitiesWith<Flame>();
    const SceneSnapshot snapshot( scene, f.mComponentTypes );

    snapshot.Restore( scene, f.mComponentTypes );
    CHECK( scene.EntitiesWith<Flame>().empty() );
    // Built again from the data, in the order a world's module would.
    for ( const Entity entity : scene.EntitiesWith<Lantern>() )
        scene.Add<Flame>( entity );
    CHECK( scene.EntitiesWith<Flame>() != flames );
    snapshot.Reorder( scene );
    CHECK( scene.EntitiesWith<Flame>() == flames );
}

TEST_CASE( "A snapshot of a type the scene's ComponentTypes do not know does not come back, and the scene stays" )
{
    Fixture f;
    const SceneSnapshot snapshot( f.mScene, f.mComponentTypes );
    Reflection reflection;
    const ComponentTypes others( reflection );
    const string saved = *TrySaveScene( f.mScene, f.mComponentTypes );
    CHECK_THROWS_WITH_AS( snapshot.Restore( f.mScene, others ),
                          "SceneSnapshot::Restore: lantern is no component type here", logic_error );
    CHECK( *TrySaveScene( f.mScene, f.mComponentTypes ) == saved );
}
