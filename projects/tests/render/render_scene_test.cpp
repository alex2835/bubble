// The render module: the camera, light and model components, and the
// RenderScene it gathers from them once a frame - no GPU needed.
#include "bubble/render/render_scene.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/types/stream.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
constexpr f32 cQuarterTurn = 1.5707964f;

Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

bool Near( vec3 a, vec3 b )
{
    return glm::length( a - b ) < 1e-5f;
}

string Triangle()
{
    ModelMesh mesh;
    mesh.mVertices = vector<ModelVertex>( 3 );
    mesh.mLods = vector<ModelLod>{ ModelLod{ .mError = 0.0f, .mIndices = { 0, 1, 2 } } };
    return WriteModelAsset( ModelAsset{ .mMin = vec3( 0.0f ), .mMax = vec3( 0.0f ), .mMeshes = { mesh } } );
}

// App with the transform and render modules, and a registry over an
// import cache in memory holding two models. The cache before App: Jobs
// finishes the loads still waiting when it goes, and they read it.
struct Fixture
{
    hmap<string, string> mImported;
    AssetCatalog mCatalog;
    App mApp{ AppOptions{ .mThreads = 1 } };
    AssetRegistry mAssets{ mCatalog, AssetRegistry::AtOnce( [this]( const AssetId& assetId, string_view kind ) {
                               const auto found = mImported.find( ImportedPath( assetId, kind ).View() );
                               if ( found == mImported.end() )
                                   return expected<string, string>( unexpected( "no such file"s ) );
                               return expected<string, string>( found->second );
                           } ),
                           mApp.GetJobs() };
    AssetId mRock = Import( "models/rock.glb" );
    AssetId mTree = Import( "models/tree.glb" );

    Fixture()
    {
        RegisterModelLoader( mAssets );
        mApp.AddModule( TransformModule() );
        mApp.AddModule( RenderModule( mAssets ) );
    }

    AssetId Import( string_view path )
    {
        const AssetId assetId = AssetId::Generate();
        mCatalog.Add( assetId, *AssetPath::TryFrom( path ) );
        mImported[ImportedPath( assetId, cModelKind ).String()] = Triangle();
        return assetId;
    }

    // A sun looking down, a lamp, a spot light, a camera, a rock and a
    // model with no asset chosen.
    Scene Level()
    {
        Scene scene;
        const Entity sun = CreateChild( scene, scene.Root(), Make( "sun" ) );
        scene.Add<Transform>( sun, Transform{ .mRotation = { -cQuarterTurn, 0.0f, 0.0f } } );
        scene.Add<DirectionalLight>( sun, DirectionalLight{ .mColor = {}, .mIntensity = 2.0f, .mShadows = true } );
        const Entity lamp = CreateChild( scene, scene.Root(), Make( "lamp" ) );
        scene.Add<Transform>( lamp, Transform{ .mPosition = { 0.0f, 3.0f, 0.0f } } );
        scene.Add<PointLight>( lamp, PointLight{ .mColor = { .mR = 1.0f, .mG = 0.5f, .mB = 0.0f, .mA = 1.0f },
                                                 .mIntensity = 4.0f,
                                                 .mRange = 6.0f } );
        scene.Add<SpotLight>( CreateChild( scene, scene.Root(), Make( "spot" ) ) );
        const Entity eye = CreateChild( scene, scene.Root(), Make( "eye" ) );
        scene.Add<Transform>( eye, Transform{ .mPosition = { 0.0f, 1.0f, 5.0f } } );
        scene.Add<Camera>( eye, Camera{ .mFov = 1.0f, .mNear = 0.5f, .mFar = 100.0f } );
        const Entity rock = CreateChild( scene, scene.Root(), Make( "rock" ) );
        scene.Add<Transform>( rock, Transform{ .mPosition = { 2.0f, 0.0f, 0.0f } } );
        scene.Add<Model>( rock, Model{ .mAsset = mRock } );
        scene.Add<Model>( CreateChild( scene, scene.Root(), Make( "unset" ) ) );
        return scene;
    }

    Entity Find( World& world, string_view path )
    {
        return *TryResolve( world.GetScene(), EntityPath{ Make( path ) } );
    }
};

const RenderLight& LightOf( const RenderScene& scene, RenderLightKind kind )
{
    for ( const RenderLight& light : scene.mLights )
        if ( light.mKind == kind )
            return light;
    FAIL( "no such light" );
    return scene.mLights.front();
}
}

TEST_CASE( "A world's cameras, lights and models are gathered once a frame" )
{
    Fixture f;
    World world( f.mApp, f.Level() );
    world.Frame( 1.0 / 60.0 );
    const RenderScene& scene = world.Get<RenderScene>();

    REQUIRE( scene.mLights.size() == 3 );
    const RenderLight& sun = LightOf( scene, RenderLightKind::Directional );
    CHECK( Near( sun.mDirection, vec3( 0.0f, -1.0f, 0.0f ) ) );
    CHECK( Near( sun.mRadiance, vec3( 2.0f ) ) );
    CHECK( sun.mShadows );
    const RenderLight& lamp = LightOf( scene, RenderLightKind::Point );
    CHECK( Near( lamp.mPosition, vec3( 0.0f, 3.0f, 0.0f ) ) );
    // Half orange as picked is a fifth in linear light.
    CHECK( Near( lamp.mRadiance, vec3( 4.0f, 4.0f * SrgbToLinear( 0.5f ), 0.0f ) ) );
    CHECK( lamp.mRange == 6.0f );
    CHECK( lamp.mOuterCos == -1.0f );
    const RenderLight& spot = LightOf( scene, RenderLightKind::Spot );
    CHECK( spot.mInnerCos == doctest::Approx( cos( SpotLight().mInnerAngle ) ) );
    CHECK( spot.mOuterCos < spot.mInnerCos );

    REQUIRE( scene.mCameras.size() == 1 );
    CHECK( Near( vec3( scene.mCameras[0].mWorld[3] ), vec3( 0.0f, 1.0f, 5.0f ) ) );
    CHECK( scene.mCameras[0].mFov == 1.0f );
    CHECK( scene.mCameras[0].mNear == 0.5f );

    REQUIRE( scene.mItems.size() == 2 );
    const RenderItem& rock = scene.mItems[0].mModel ? scene.mItems[0] : scene.mItems[1];
    const RenderItem& unset = scene.mItems[0].mModel ? scene.mItems[1] : scene.mItems[0];
    CHECK( rock.mEntity == f.Find( world, "/rock" ) );
    CHECK( Near( vec3( rock.mWorld[3] ), vec3( 2.0f, 0.0f, 0.0f ) ) );
    CHECK( rock.mModel.Entry().Id() == f.mRock );
    // Loaded on the pool, there at the main thread's next turn.
    CHECK( rock.mModel.State() == AssetState::Loading );
    f.mApp.GetJobs().RunMainQueue();
    CHECK( rock.mModel.Ready() );
    CHECK_FALSE( unset.mModel );
}

TEST_CASE( "A model's asset set anew is loaded, and a model taken away is not drawn" )
{
    Fixture f;
    World world( f.mApp, f.Level() );
    const Entity rock = f.Find( world, "/rock" );
    const ComponentTypes& componentTypes = f.mApp.GetComponentTypes();
    REQUIRE( componentTypes.TrySetField( world.GetScene(), rock, *componentTypes.TryFind( "model"_n ),
                                         *FieldPath::TryParse( "asset" ), MetaAny( f.mTree ) ) );
    world.Frame( 1.0 / 60.0 );
    const RenderScene& scene = world.Get<RenderScene>();
    bool found = false;
    for ( const RenderItem& item : scene.mItems )
        if ( item.mEntity == rock )
        {
            found = true;
            CHECK( item.mModel.Entry().Id() == f.mTree );
        }
    CHECK( found );

    world.GetScene().Remove<Model>( rock );
    world.Frame( 1.0 / 60.0 );
    for ( const RenderItem& item : world.Get<RenderScene>().mItems )
        CHECK( item.mEntity != rock );
}
