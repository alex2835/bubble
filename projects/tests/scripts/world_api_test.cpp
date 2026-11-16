// What a world's scripts reach of the engine through reflection: each
// component through its entity, written so the world hears of it; the
// services by name; the input's actions as the step or the frame sees them.
#include "bubble/scripts/script_declarations.hpp"
#include "scripts/script_world.hpp"
#include <doctest.h>

using namespace bubble;
using namespace bubble::test;

namespace
{
// A world of one entity, "it", with a transform and the script.
struct OneScript
{
    ScriptWorld mWorld;
    Entity mEntity;
    Scope<World> mLive;

    OneScript( string_view source, bool render = true ) : mWorld( render )
    {
        const AssetId script = mWorld.Write( "scripts/it.luau", source );
        Scene scene;
        mEntity = ScriptWorld::Add( scene, "it", script );
        scene.Add<Transform>( mEntity );
        mLive = CreateScope<World>( mWorld.mApp, std::move( scene ) );
        mWorld.Load( *mLive );
    }

    World& Live() { return *mLive; }
};
}

TEST_CASE( "A script reads and writes its entity's components by their fields, and the world hears the writes" )
{
    OneScript one( R"(
function on_fixed_update( self, dt )
    local t = self.entity.transform
    t.position = t.position + vector.create( 1, 0, 0 )
    t.scale = vector.create( 2, 2, 2 )
    record( string.format( "%g %s", t.position.x, typeof( t ) ) )
end
)" );
    u32 heard = 0;
    one.Live().OnChanged<Transform>( "position"_n, [&]( Entity ) { ++heard; } );
    one.Live().Step();
    one.Live().Step();
    CHECK( one.mWorld.TakeRecorded() == vector<string>{ "1 transform", "2 transform" } );
    const Transform& transform = one.Live().GetScene().Get<Transform>( one.mEntity );
    CHECK( transform.mPosition == vec3( 2.0f, 0.0f, 0.0f ) );
    CHECK( transform.mScale == vec3( 2.0f ) );
    // Heard at the world's next turn, once a write.
    one.Live().Frame( 0.0 );
    CHECK( heard == 2 );
}

TEST_CASE( "A component an entity lacks, a field it lacks, or a value of another kind is the script's error" )
{
    OneScript one( R"(
function on_fixed_update( self, dt )
    local e = self.entity
    local which = self.which
    if which == 1 then local _ = e.camera end
    if which == 2 then local _ = e.transform.positon end
    if which == 3 then e.transform.position = 5 end
    if which == 4 then local _ = e:has( "lamp" ) end
    if which == 5 then e:add( "transform" ) end
end
locals { which = 0 }
)" );
    const LogWatch log;
    for ( int which = 1; which <= 5; ++which )
    {
        ScriptRuntime& runtime = one.Live().Get<ScriptRuntime>();
        const ScriptInstanceHandle instanceHandle =
            one.Live().GetScene().Get<ScriptInstance>( one.mEntity ).mInstanceHandle;
        runtime.Self( instanceHandle ).RawSet( "which", which );
        one.Live().Step();
        // A failed instance waits for its file to change; this one is put
        // back on as a change would.
        one.mWorld.Change( "scripts/it.luau", one.mWorld.mFiles.at( "scripts/it.luau" ) );
    }
    CHECK( log.Saw( LogLevel::Error, { "/it has no camera (it has: script, transform)" } ) );
    CHECK( log.Saw( LogLevel::Error, { "transform has no field 'positon' (fields: position, rotation, scale)" } ) );
    CHECK( log.Saw( LogLevel::Error, { "transform.position: a vec3 expected, got 5" } ) );
    CHECK( log.Saw( LogLevel::Error, { "no component \"lamp\" (components: " } ) );
    CHECK( log.Saw( LogLevel::Error, { "/it has a transform already" } ) );
}

TEST_CASE( "A script asks whether its entity has a component, adds one and removes one" )
{
    OneScript one( R"(
function on_start( self )
    local e = self.entity
    record( tostring( e:has( "point_light" ) ) )
    e:add( "point_light" )
    e.point_light.intensity = 3
    record( tostring( e:has( "point_light" ) ) .. " " .. e.point_light.intensity )
    e:remove( "transform" )
    record( tostring( e:has( "transform" ) ) )
end
)" );
    CHECK( one.mWorld.TakeRecorded() == vector<string>{ "false", "true 3", "false" } );
    CHECK( one.Live().GetScene().Get<PointLight>( one.mEntity ).mIntensity == 3.0f );
}

TEST_CASE( "A service is called by its name, found when called: its module may come after the scripts" )
{
    OneScript one( R"(
function on_update( self, dt )
    debug_draw.line( vector.create( 0, 0, 0 ), vector.create( 1, 0, 0 ), color( 1, 0, 0 ) )
    debug_draw.sphere( vector.zero, 2, color( 0, 1, 0 ) )
end
)" );
    // The render module takes the lines at the frame's end; count them
    // before by drawing with no frame since.
    one.Live().Get<DebugDraw>().Take();
    one.Live().Get<ScriptRuntime>().Call( one.Live().GetScene().Get<ScriptInstance>( one.mEntity ).mInstanceHandle,
                                          static_cast<u32>( ScriptCallback::Update ), 0.0 );
    CHECK( one.Live().Get<DebugDraw>().Count() > 1 );

    // An App without the render module describes no debug_draw at all.
    const LogWatch log;
    OneScript bare( "function on_start( self ) debug_draw.line( vector.zero, vector.one, color( 1, 1, 1 ) ) end",
                    false );
    CHECK( log.Saw( LogLevel::Error, { "it.luau:1: attempt to index nil with 'line'" } ) );
}

TEST_CASE( "A service's method checks its arguments" )
{
    const LogWatch log;
    OneScript one( R"(
locals { which = 0 }
function on_start( self )
    debug_draw.line( vector.zero, vector.one )
end
)" );
    CHECK( log.Saw( LogLevel::Error, { "debug_draw.line takes 3 arguments, got 2" } ) );
    OneScript other( "function on_start( self ) debug_draw.line( vector.zero, 5, color( 1, 1, 1 ) ) end" );
    CHECK( log.Saw( LogLevel::Error, { "debug_draw.line: argument 2: a vec3 expected, got 5" } ) );
}

TEST_CASE( "input reads what the fixed step running sees, and the frame's otherwise; with no input all is at rest" )
{
    OneScript one( R"(
function on_fixed_update( self, dt )
    record( string.format( "step %s %s %g", tostring( input.down( "jump" ) ), tostring( input.pressed( "jump" ) ),
                           input.axis( "move_x" ) ) )
end
function on_update( self, dt )
    record( string.format( "frame %s", tostring( input.pressed( "jump" ) ) ) )
end
)" );
    (void)one.mWorld.TakeRecorded();
    one.Live().Step();
    one.Live().Frame( 0.0 );
    CHECK( one.mWorld.TakeRecorded() == vector<string>{ "step false false 0", "frame false" } );

    ActionMap map;
    map.Bind( "jump", Key::Space ).Bind( "move_x", Key::A, -1.0f ).Bind( "move_x", Key::D );
    Input input( std::move( map ) );
    one.Live().SetInput( WorldInput{ .mLatchStep = [&] { return input.LatchStep(); },
                                     .mLatchFrame = [&] { return input.LatchFrame(); } } );
    input.Handle( KeyDown{ .mKey = Key::Space, .mRepeat = false } );
    input.Handle( KeyDown{ .mKey = Key::A, .mRepeat = false } );
    one.Live().Step();
    one.Live().Step();
    one.Live().Frame( 0.0 );
    // A press is the first step's once, and the frame's once.
    CHECK( one.mWorld.TakeRecorded() == vector<string>{ "step true true -1", "step true false -1", "frame true" } );

    const LogWatch log;
    one.mWorld.Change( "scripts/it.luau", "function on_update( self, dt ) input.down( 'duck' ) end" );
    one.Live().Frame( 0.0 );
    CHECK( log.Saw( LogLevel::Error, { "input: no action \"duck\" (actions: jump, move_x)" } ) );
}

TEST_CASE( "bubble.d.luau declares the components, the entity, the services and the project's actions" )
{
    ScriptWorld w;
    const string declarations = WriteScriptDeclarations( w.mApp, ScriptLabels{ .mActions = { "jump", "move_x" } } );
    const auto has = [&]( string_view part ) {
        INFO( part );
        CHECK( declarations.find( part ) != string::npos );
    };
    has( "declare extern type transform with\n    position: vector\n    rotation: vector\n    scale: vector\nend" );
    has( "declare extern type point_light with\n    color: color\n    intensity: number\n    range: number\nend" );
    has( "declare extern type script with\n    asset: asset\n    props: unknown\nend" );
    has( "    parent: entity?\n" );
    has( "    transform: transform\n" );
    has( "    function find(self, path: string): entity?\n" );
    has( "type action = \"jump\" | \"move_x\"\n" );
    has( "    line: (from: vector, to: vector, color: color) -> (),\n" );
    has( "    -- Three circles, one about each axis.\n    sphere: (centre: vector, radius: number, color: color) -> "
         "(),\n" );
    has( "declare function props<T>(props: T): T\n" );
    // With no actions declared an action is any string.
    CHECK( WriteScriptDeclarations( w.mApp ).find( "type action = string\n" ) != string::npos );
}
