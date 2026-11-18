// The animation module: a model's skeleton and clips, imported from one
// source, played by an animation graph - poses between steps, the skin the
// renderer draws, parameters and events through scripts, eased transitions,
// what hangs on joints, snapshots that play on the same.
#include "bubble/import/model_importer.hpp"
#ifdef BUBBLE_IMPORT_MODELS
#include "animation/skinned_model.hpp"
#include "bubble/animation/animation.hpp"
#include "bubble/animation/animation_assets.hpp"
#include "bubble/import/animation_graph_importer.hpp"
#include "bubble/import/import_cache.hpp"
#include "bubble/import/script_importer.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scripts/script_declarations.hpp"
#include "bubble/scripts/script_module.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"
#include "log_watch.hpp"
#include <doctest.h>

using namespace bubble;
using namespace bubble::test;

namespace
{
struct AnimationFixture
{
    // Before App: Jobs finishes the loads still waiting when it goes.
    hmap<string, string> mFiles;
    AssetCatalog mCatalog;
    App mApp{ AppOptions{ .mThreads = 1 } };
    AssetRegistry mAssets{ mCatalog, AssetRegistry::AtOnce( [this]( const AssetId& assetId, string_view kind ) {
                               const auto found = mFiles.find( ImportedPath( assetId, kind ).View() );
                               if ( found == mFiles.end() )
                                   return expected<string, string>( unexpected( "no such file"s ) );
                               return expected<string, string>( found->second );
                           } ),
                           mApp.GetJobs() };
    vector<string> mRecorded;

    AnimationFixture()
    {
        RegisterScriptLoader( mAssets );
        RegisterModelLoader( mAssets );
        RegisterAnimationLoaders( mAssets );
        mApp.AddModule( TransformModule() );
        mApp.AddModule( RenderModule( mAssets ) );
        mApp.AddModule(
            ScriptModule( mAssets, ScriptModuleOptions{ .mGlobals =
                                                            [this]( LuaState& lua, World& ) {
                                                                lua.Globals()["record"] = [this]( string text ) {
                                                                    mRecorded.push_back( std::move( text ) );
                                                                };
                                                            },
                                                        .mAliases = {},
                                                        .mTimeLimit = 1.0,
                                                        .mProgress = {},
                                                        .mSaves = {},
                                                        .mGameVersion = {} } ) );
        mApp.AddModule( AnimationModule( mAssets ) );
    }

    AssetId Import( string_view path, string source, const Importer& importer, string_view settings = "{}" )
    {
        mFiles[string( path )] = std::move( source );
        const AssetPath assetPath = *AssetPath::TryFrom( path );
        if ( not mCatalog.TryIdOf( assetPath ) )
            mCatalog.Add( AssetId::Generate(), assetPath );
        const AssetId assetId = *mCatalog.TryIdOf( assetPath );
        const auto importedMaybe = TryImportAsset( ProjectFiles::InMemory( mFiles ), importer, assetPath, assetId,
                                                   *Json::TryParse( settings ) );
        REQUIRE_MESSAGE( importedMaybe, importedMaybe.error() );
        return assetId;
    }

    AssetId ImportModel( string_view path, string source, string_view settings = "{}" )
    {
        for ( const Importer& importer : ModelImporters() )
            if ( AssetPath::TryFrom( path )->Extension() == importer.mExtension )
                return Import( path, std::move( source ), importer, settings );
        FAIL( "no importer for ", path );
        return {};
    }

    AssetId Strip() { return ImportModel( "models/strip.glb", SkinnedGlb() ); }

    AssetId Graph( string_view path, string_view text )
    {
        return Import( path, string( text ), AnimationGraphImporter() );
    }

    AssetId Script( string_view path, string_view source )
    {
        return Import( path, string( source ), ScriptImporter() );
    }

    static Entity Place( Scene& scene, string_view name, Transform transform, Entity parent = Entity::Null )
    {
        const Entity entity =
            CreateChild( scene, parent == Entity::Null ? scene.Root() : parent, *Text::TryFrom( name ) );
        scene.Add<Transform>( entity, transform );
        return entity;
    }

    // An entity drawing `model`, animated by `graph`.
    static Entity Animated( Scene& scene,
                            AssetId model,
                            AssetId graph,
                            vec3 position = vec3( 0.0f ),
                            string_view name = "strip" )
    {
        const Entity entity = Place( scene, name, Transform{ .mPosition = position } );
        Model drawn;
        drawn.mAsset = model;
        scene.Add<Model>( entity, drawn );
        scene.Add<Animator>( entity, Animator{ .mGraph = graph, .mSpeed = 1 } );
        return entity;
    }

    // Frames with no time until what the world asked for has loaded: no
    // step is taken.
    void Settle( World& world )
    {
        for ( u32 turn = 0; turn < 4; ++turn )
        {
            mApp.GetJobs().RunMainQueue();
            world.Frame( 0.0 );
        }
    }

    // Steps, then a frame of no time: the pose of the step before the last.
    void Steps( World& world, u32 count )
    {
        for ( u32 step = 0; step < count; ++step )
        {
            mApp.GetJobs().RunMainQueue();
            world.Step();
        }
        world.Frame( 0.0 );
    }

    void Frames( World& world, u32 count )
    {
        for ( u32 frame = 0; frame < count; ++frame )
        {
            mApp.GetJobs().RunMainQueue();
            world.Frame( 1.0 / 60.0 );
        }
    }
};

// How far a matrix turns about Z.
f32 AngleZ( const mat4& matrix )
{
    return atan2( matrix[0][1], matrix[0][0] );
}

constexpr string_view cBend = R"({ "states": { "bend": { "clip": "bend" } } })";

constexpr string_view cBlend = R"({
  "parameters": { "mix": 0 },
  "states": { "mixed": { "blend": { "param": "mix", "points": [ ["bend", 0], ["lean", 1] ] } } } })";
}

TEST_CASE( "An animator plays its clip on its model's skeleton, the skin posed as the step before the last" )
{
    AnimationFixture f;
    const AssetId model = f.Strip();
    const AssetId graph = f.Graph( "anims/bend.anim", cBend );
    Scene scene;
    const Entity strip = AnimationFixture::Animated( scene, model, graph );
    World world( f.mApp, std::move( scene ) );
    f.Settle( world );
    AnimationWorld& animation = world.Get<AnimationWorld>();
    CHECK_FALSE( animation.TryState( strip ) );

    // 31 steps: the pose at 30 of 60 - halfway through bend's second.
    f.Steps( world, 31 );
    CHECK( animation.TryState( strip ) == "bend"s );
    const auto upperMaybe = animation.TryJoint( strip, "upper" );
    REQUIRE( upperMaybe );
    // Near enough: ozz keeps a clip's keys in 16 bits.
    CHECK( AngleZ( *upperMaybe ) == doctest::Approx( glm::radians( 45.0f ) ).epsilon( 0.005 ) );
    CHECK( glm::length( vec3( ( *upperMaybe )[3] ) - vec3( 0.0f, 1.0f, 0.0f ) ) < 1e-3f );
    CHECK_FALSE( animation.TryJoint( strip, "tail" ) );

    // The skin: each joint's pose times its inverse bind. The top turns
    // about upper.
    const SkinPose& skin = world.GetScene().Get<SkinPose>( strip );
    REQUIRE( skin.mMatrices.size() == 3 );
    const vec3 top = vec3( skin.mMatrices[2] * vec4( 0.1f, 2.0f, 0.0f, 1.0f ) );
    const vec3 expected =
        vec3( 0.0f, 1.0f, 0.0f ) + glm::angleAxis( glm::radians( 45.0f ), vec3( 0, 0, 1 ) ) * vec3( 0.1f, 1.0f, 0.0f );
    CHECK( glm::length( top - expected ) < 2e-3f );
    // As the renderer gets it.
    const RenderScene& render = world.Get<RenderScene>();
    REQUIRE( render.mItems.size() == 1 );
    REQUIRE( render.mItems[0].mSkinCount == 3 );
    CHECK( render.mSkins[render.mItems[0].mSkinFirst + 2] == skin.mMatrices[2] );

    // Its speed: 0 stands.
    world.GetScene().Get<Animator>( strip ).mSpeed = 0;
    f.Steps( world, 10 );
    CHECK( AngleZ( *animation.TryJoint( strip, "upper" ) ) ==
           doctest::Approx( glm::radians( 46.5f ) ).epsilon( 0.005 ) );
}

TEST_CASE( "A blend weighs its clips by a parameter a script sets, and a script hears the events passed" )
{
    AnimationFixture f;
    const AssetId model = f.Strip();
    const AssetId graph = f.Graph( "anims/mix.anim", R"({
      "parameters": { "mix": 0, "bent": false, "wave": "trigger" },
      "states": { "rest": {},
                  "mixed": { "blend": { "param": "mix", "points": [ ["bend", 0], ["lean", 1] ] } },
                  "lean": { "clip": "lean", "loop": false } },
      "transitions": [ { "from": "rest", "to": "mixed", "when": "bent", "duration": 0 },
                       { "from": "*", "to": "lean", "when": "wave", "duration": 0 },
                       { "from": "lean", "to": "return", "exit_time": 1, "duration": 0 } ],
      "events": { "bend": [ [0.45, "mid"] ], "lean": [ [0.25, "quarter"] ] } })" );
    const AssetId script = f.Script( "scripts/dancer.luau", R"(
function on_start( self )
    self.entity.animator:set( "bent", true )
    self.entity.animator:set( "mix", 0.4 )
end
function on_animation_event( self, name: string )
    record( name .. " " .. tostring( self.entity.animator.state ) )
end
)" );
    Scene scene;
    const Entity strip = AnimationFixture::Animated( scene, model, graph );
    scene.Add<Script>( strip, Script{ .mAsset = script } );
    World world( f.mApp, std::move( scene ) );
    f.Settle( world );

    f.Steps( world, 31 );
    AnimationWorld& animation = world.Get<AnimationWorld>();
    CHECK( animation.TryState( strip ) == "mixed"s );
    // Lower leans 0.4 of lean's turn, upper bends 0.6 of bend's: each clip
    // pins the joint the other turns at rest.
    const mat4 lower = *animation.TryJoint( strip, "lower" );
    const mat4 upper = *animation.TryJoint( strip, "upper" );
    const f32 phase = 29.0f / 60.0f;
    CHECK( AngleZ( lower ) == doctest::Approx( -glm::radians( 90.0f * phase ) * 0.4f ).epsilon( 0.02 ) );
    CHECK( AngleZ( glm::inverse( lower ) * upper ) ==
           doctest::Approx( glm::radians( 90.0f * phase ) * 0.6f ).epsilon( 0.02 ) );
    // Bend leads the blend: its event is heard.
    CHECK( f.mRecorded == vector<string>{ "mid mixed" } );

    // A trigger from outside: the one-shot plays to its end and returns.
    REQUIRE( animation.TryTrigger( strip, "wave" ) );
    f.Steps( world, 1 );
    CHECK( animation.TryState( strip ) == "lean"s );
    f.Steps( world, 65 );
    CHECK( animation.TryState( strip ) == "mixed"s );
    CHECK( f.mRecorded == vector<string>{ "mid mixed", "quarter lean" } );

    CHECK( animation.TrySet( strip, "nope", 1.0f ).error() == "no parameter nope (parameters: mix, bent, wave)" );
    CHECK( animation.TrySet( strip, "bent", 2.0f ).error() == "bent is true or false, not 2" );
    CHECK( animation.TrySet( strip, "mix", true ).error() == "mix is a number, not true" );
    CHECK( animation.TrySet( strip, "wave", true ).error() == "wave is a trigger: trigger it instead" );
    CHECK( animation.TryTrigger( strip, "bent" ).error() == "bent is not a trigger: set it instead" );
    CHECK( animation.TrySet( world.GetScene().Root(), "mix", 1.0f ).error() == "/ has no animator" );
}

TEST_CASE( "A transition eases the pose out of the state before rather than cutting to the new one" )
{
    AnimationFixture f;
    const AssetId model = f.Strip();
    const AssetId graph = f.Graph( "anims/relax.anim", R"({
      "parameters": { "relax": "trigger" },
      "states": { "bend": { "clip": "bend", "loop": false }, "rest": {} },
      "transitions": [ { "from": "bend", "to": "rest", "when": "relax", "duration": 0.5 } ] })" );
    Scene scene;
    const Entity strip = AnimationFixture::Animated( scene, model, graph );
    World world( f.mApp, std::move( scene ) );
    f.Settle( world );
    AnimationWorld& animation = world.Get<AnimationWorld>();
    // Bent all the way, and held there.
    f.Steps( world, 70 );
    CHECK( AngleZ( *animation.TryJoint( strip, "upper" ) ) ==
           doctest::Approx( glm::radians( 90.0f ) ).epsilon( 0.005 ) );

    REQUIRE( animation.TryTrigger( strip, "relax" ) );
    f.Steps( world, 1 );
    CHECK( animation.TryState( strip ) == "rest"s );
    // Where it was still, then on its way, then at rest.
    CHECK( AngleZ( *animation.TryJoint( strip, "upper" ) ) ==
           doctest::Approx( glm::radians( 90.0f ) ).epsilon( 0.005 ) );
    f.Frames( world, 12 );
    const f32 halfway = AngleZ( *animation.TryJoint( strip, "upper" ) );
    CHECK( halfway > glm::radians( 5.0f ) );
    CHECK( halfway < glm::radians( 85.0f ) );
    f.Frames( world, 30 );
    CHECK( abs( AngleZ( *animation.TryJoint( strip, "upper" ) ) ) < 1e-4f );
}

TEST_CASE( "A bone attachment hangs its entity on a joint of its parent's pose" )
{
    LogWatch log;
    AnimationFixture f;
    const AssetId model = f.Strip();
    const AssetId graph = f.Graph( "anims/bend.anim", cBend );
    Scene scene;
    const Entity strip = AnimationFixture::Animated( scene, model, graph, vec3( 2.0f, 0.0f, 0.0f ) );
    const Entity sword =
        AnimationFixture::Place( scene, "sword", Transform{ .mPosition = { 0.0f, 0.5f, 0.0f } }, strip );
    scene.Add<BoneAttachment>( sword, BoneAttachment{ .mBone = "upper" } );
    const Entity lost = AnimationFixture::Place( scene, "lost", Transform{ .mPosition = { 0.0f, 0.5f, 0.0f } }, strip );
    scene.Add<BoneAttachment>( lost, BoneAttachment{ .mBone = "tail" } );
    World world( f.mApp, std::move( scene ) );
    f.Settle( world );
    f.Steps( world, 31 );

    const vec3 held = vec3( world.GetScene().Get<WorldTransform>( sword ).mMatrix[3] );
    const vec3 expected =
        vec3( 2.0f, 1.0f, 0.0f ) + glm::angleAxis( glm::radians( 45.0f ), vec3( 0, 0, 1 ) ) * vec3( 0.0f, 0.5f, 0.0f );
    CHECK( glm::length( held - expected ) < 2e-3f );
    // A joint the skeleton lacks: hung from the parent, and said once.
    CHECK( vec3( world.GetScene().Get<WorldTransform>( lost ).mMatrix[3] ) == vec3( 2.0f, 0.5f, 0.0f ) );
    f.Steps( world, 5 );
    size_t said = 0;
    for ( const LogEntry& entry : log.Entries() )
        said += entry.mText.find( "/strip/lost: bone_attachment: models/strip.glb has no joint tail" ) != string::npos;
    CHECK( said == 1 );

    // Taken off, it hangs from its parent again.
    world.GetScene().Remove<BoneAttachment>( sword );
    f.Steps( world, 1 );
    CHECK( vec3( world.GetScene().Get<WorldTransform>( sword ).mMatrix[3] ) == vec3( 2.0f, 0.5f, 0.0f ) );
}

TEST_CASE( "A world restored from a snapshot plays its animators on as the one it was taken from" )
{
    AnimationFixture f;
    const AssetId model = f.Strip();
    const AssetId graph = f.Graph( "anims/mix.anim", cBlend );
    Scene scene;
    const Entity strip = AnimationFixture::Animated( scene, model, graph );
    World world( f.mApp, std::move( scene ) );
    f.Settle( world );
    AnimationWorld& animation = world.Get<AnimationWorld>();
    REQUIRE( animation.TrySet( strip, "mix", 0.25f ) );
    f.Steps( world, 20 );
    const WorldSnapshot snapshot = world.Snapshot();

    f.Steps( world, 25 );
    const mat4 lower = *animation.TryJoint( strip, "lower" );
    const mat4 upper = *animation.TryJoint( strip, "upper" );
    REQUIRE( animation.TrySet( strip, "mix", 1.0f ) );
    f.Steps( world, 5 );

    world.Restore( snapshot );
    f.Steps( world, 25 );
    CHECK( *animation.TryJoint( strip, "lower" ) == lower );
    CHECK( *animation.TryJoint( strip, "upper" ) == upper );
}

TEST_CASE( "What keeps an animator from playing is said once, and a clip the model lacks holds the rest pose" )
{
    LogWatch log;
    AnimationFixture f;
    const AssetId strip = f.Strip();
    const AssetId box = f.ImportModel( "models/box.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n" );
    const AssetId running = f.Graph( "anims/run.anim", R"({ "states": { "run": { "clip": "run" } } })" );
    Scene scene;
    const Entity unskinned = AnimationFixture::Animated( scene, box, running, vec3( 0.0f ), "box" );
    const Entity missing = AnimationFixture::Animated( scene, strip, running );
    const Entity bare = AnimationFixture::Place( scene, "bare", Transform{} );
    scene.Add<Animator>( bare, Animator{ .mGraph = running, .mSpeed = 1 } );
    World world( f.mApp, std::move( scene ) );
    f.Settle( world );
    f.Steps( world, 10 );

    const auto count = [&]( string_view text ) {
        size_t seen = 0;
        for ( const LogEntry& entry : log.Entries() )
            seen += entry.mText.find( text ) != string::npos;
        return seen;
    };
    CHECK( count( "animator: models/box.obj has no skin, and so no skeleton" ) == 1 );
    CHECK( count( "animator: models/strip.glb has no clip run (clips: bend, lean, walk)" ) == 1 );
    CHECK( count( "/bare: animator: its entity has no model to animate" ) == 1 );
    AnimationWorld& animation = world.Get<AnimationWorld>();
    CHECK_FALSE( animation.TryState( unskinned ) );
    CHECK_FALSE( world.GetScene().Has<SkinPose>( unskinned ) );
    // Played still, at rest.
    CHECK( animation.TryState( missing ) == "run"s );
    CHECK( AngleZ( *animation.TryJoint( missing, "upper" ) ) == 0.0f );
}

TEST_CASE( "A state with root motion says how fast its clips' root goes, in the world; the clip plays on the spot" )
{
    AnimationFixture f;
    const AssetId model = f.ImportModel( "models/walker.glb", SkinnedGlb(), R"({ "root_motion": "lower" })" );
    const AssetId graph = f.Graph( "anims/walk.anim", R"({
      "parameters": { "stop": false },
      "states": { "walk": { "clip": "walk", "root_motion": true }, "shuffle": { "clip": "walk" } },
      "transitions": [ { "from": "walk", "to": "shuffle", "when": "stop", "duration": 0 } ] })" );
    const AssetId script = f.Script( "scripts/walker.luau", R"(
function on_fixed_update( self, dt: number )
    local velocity = self.entity.animator.root_velocity
    record( string.format( "%.2f %.2f %.2f", velocity.x, velocity.y, velocity.z ) )
end
)" );
    Scene scene;
    const Entity walker = AnimationFixture::Animated( scene, model, graph );
    // Turned to face along X: the clip's +X is the world's -Z.
    scene.Get<Transform>( walker ).mRotation = vec3( 0.0f, glm::radians( 90.0f ), 0.0f );
    scene.Add<Script>( walker, Script{ .mAsset = script } );
    World world( f.mApp, std::move( scene ) );
    f.Settle( world );
    f.Steps( world, 20 );

    AnimationWorld& animation = world.Get<AnimationWorld>();
    const AnimationRootMotion motion = animation.RootMotion( walker );
    CHECK( glm::length( motion.mVelocity - vec3( 0.0f, 0.0f, -1.0f ) ) < 0.01f );
    CHECK( motion.mTurn == doctest::Approx( 0.0f ) );
    // A script reads what the step before it asked for.
    REQUIRE( not f.mRecorded.empty() );
    CHECK( f.mRecorded.back() == "0.00 0.00 -1.00" );
    // On the spot: lower stays where it rests.
    CHECK( glm::length( vec3( ( *animation.TryJoint( walker, "lower" ) )[3] ) ) < 1e-3f );

    // A state without root motion moves nothing, on the spot as well.
    REQUIRE( animation.TrySet( walker, "stop", true ) );
    f.Steps( world, 5 );
    CHECK( animation.TryState( walker ) == "shuffle"s );
    CHECK( animation.RootMotion( walker ).mVelocity == vec3( 0.0f ) );
    CHECK( glm::length( vec3( ( *animation.TryJoint( walker, "lower" ) )[3] ) ) < 1e-3f );
}

TEST_CASE( "A Z-up rig's root turn comes out about the model's up, and its lean stays in the clip" )
{
    AnimationFixture f;
    const AssetId model = f.ImportModel( "models/turner.glb", SkinnedGlb( true ), R"({ "root_motion": "lower" })" );
    const AssetId graph = f.Graph(
        "anims/turn.anim", R"({ "states": { "turn": { "clip": "turn", "root_motion": true, "loop": false } } })" );
    Scene scene;
    const Entity turner = AnimationFixture::Animated( scene, model, graph );
    World world( f.mApp, std::move( scene ) );
    f.Settle( world );
    f.Steps( world, 30 );
    AnimationWorld& animation = world.Get<AnimationWorld>();
    // Halfway, about the up at nearly a quarter turn a second - the keys are
    // eased between as quaternions, not as angles.
    CHECK( animation.RootMotion( turner ).mTurn == doctest::Approx( glm::radians( 90.0f ) ).epsilon( 0.1 ) );

    // Ended, it leans toward the model's +Z as the clip leans it - not turned
    // aside, and not tipped over by a turn taken for a roll.
    f.Steps( world, 40 );
    const vec3 upper( ( *animation.TryJoint( turner, "upper" ) )[3] );
    CHECK( glm::length( upper - vec3( 0.0f, 0.8660254f, 0.5f ) ) < 0.01f );
    CHECK( animation.RootMotion( turner ).mTurn == doctest::Approx( 0.0f ) );
}

TEST_CASE( "What animation gives scripts is declared, and a typed script using it passes the strict check" )
{
    AnimationFixture f;
    const string declarations = WriteScriptDeclarations( f.mApp );
    CHECK( declarations.find( "    function set(self, parameter: string, value: number | boolean): ()\n" ) !=
           string::npos );
    CHECK( declarations.find( "    state: string?\n    root_velocity: vector\n    root_turn: number\n" ) !=
           string::npos );
    CHECK( declarations.find( "--     on_animation_event( self, name: string )\n" ) != string::npos );

    const Importer importer =
        ScriptImporter( ScriptCheck{ .mDeclarations = declarations, .mAliases = {}, .mRead = {}, .mAssetOf = {} } );
    const string source = R"(
local P = props { speed = 3 }
type Self = typeof( P ) & { entity: entity }

function on_fixed_update( self: Self, dt: number )
    local animator = self.entity.animator
    animator:set( "speed", self.speed )
    animator:set( "grounded", true )
    if animator.state == "idle" then
        animator:trigger( "wave" )
    end
    animator.speed = 2
end

function on_animation_event( self: Self, name: string )
    print( name )
end
)";
    const auto importedMaybe = importer.mImport( source, Json(), *AssetPath::TryFrom( "scripts/hero.luau" ) );
    REQUIRE_MESSAGE( importedMaybe, importedMaybe.error() );
    string wrong = source;
    wrong.replace( wrong.find( "animator:trigger( \"wave\" )" ), 26, "animator:trigger( 1 )" );
    CHECK_FALSE( importer.mImport( wrong, Json(), *AssetPath::TryFrom( "scripts/hero.luau" ) ) );
}
#endif
