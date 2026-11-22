// The editor's project and the worlds that show its documents: a project in
// memory opened with every module, its start level a document, the
// document's world showing it with the modules that show a scene alone, an
// edit heard there, the world gone with the document. And the editor's
// camera.
#include "bubble/audio/audio.hpp"
#include "bubble/editor/document_world.hpp"
#include "bubble/editor/editor_camera.hpp"
#include "bubble/editor/editor_project.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scripts/script_module.hpp"
#include "bubble/world/transform.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
constexpr string_view cProject = R"({
  "version": 1,
  "name": "test",
  "start_level": { "path": "levels/main.scene" },
  "actions": {}
})";

constexpr string_view cLevel = R"({
  "version": 1,
  "root": {
    "children": [
      {
        "name": "lamp",
        "components": {
          "transform": { "position": [1, 2, 3] },
          "point_light": { "color": [1, 1, 1, 1], "intensity": 5, "range": 10 },
          "script": { "asset": { "asset": "5c0a1e5c0a1e5c0a1e5c0a1e5c0a1e5c", "path": "scripts/sink.luau" } }
        }
      }
    ]
  }
})";

// Were it running, the lamp would sink.
constexpr string_view cScriptImport =
    R"({ "version": 1, "asset": "5c0a1e5c0a1e5c0a1e5c0a1e5c0a1e5c", "settings": {} })";

constexpr string_view cScript = R"(type Self = { entity: entity }

function on_update( self: Self, dt: number )
    self.entity.transform.position = vector.create( 0, -10, 0 )
end
)";

Json Args( string_view json )
{
    return *Json::TryParse( json );
}
}

TEST_CASE( "A document's world shows it with the modules that show a scene, hears its edits, and goes with it" )
{
    hmap<string, string> files{ { "project.bubble", string( cProject ) },
                                { "levels/main.scene", string( cLevel ) },
                                { "scripts/sink.luau", string( cScript ) },
                                { "scripts/sink.luau.import", string( cScriptImport ) } };
    const ProjectFiles projectFiles = ProjectFiles::InMemory( files );
    const Scope<AudioDevice> audio = AudioDevice::Offline();
    auto projectMaybe = EditorProject::TryOpen(
        projectFiles, AssetRegistry::AtOnce( [projectFiles]( const AssetId& assetId, string_view kind ) {
            return projectFiles.TryRead( ImportedPath( assetId, kind ) );
        } ),
        *audio );
    REQUIRE_MESSAGE( projectMaybe, projectMaybe.error() );
    EditorProject& project = **projectMaybe;
    CHECK( project.GetSettings().mName == "test" );
    const auto openedMaybe = project.TryOpenStartLevel();
    REQUIRE_MESSAGE( openedMaybe, openedMaybe.error() );
    Workspace& workspace = project.GetWorkspace();
    Document& document = *workspace.TryActive();
    CHECK( document.TryPath()->View() == "levels/main.scene" );

    World& world = WorldOf( project.GetApp(), document );
    CHECK( &WorldOf( project.GetApp(), document ) == &world );
    CHECK( &world.GetScene() == &document.GetScene() );
    CHECK( world.Editing() );
    world.Frame( 0.25 );
    const Entity lamp = *TryResolve( document.GetScene(), EntityPath{ *Text::TryFrom( "/lamp" ) } );
    CHECK( not document.GetScene().Has<ScriptInstance>( lamp ) );
    CHECK( document.GetScene().Get<Transform>( lamp ).mPosition == vec3( 1, 2, 3 ) );
    const auto light = [&] { return world.Get<RenderScene>().mLights.at( 0 ); };
    CHECK( light().mPosition == vec3( 1, 2, 3 ) );
    CHECK( light().mRadiance.x == doctest::Approx( 5 ) );

    // An edit is a step of the document's history, and the world sees it.
    REQUIRE( project.Run( "property.set",
                          Args( R"({ "entities": [ "/lamp" ], "component": "point_light", "path": "intensity",
                                     "value": 9 })" ) ) );
    world.Frame( 0.25 );
    CHECK( light().mRadiance.x == doctest::Approx( 9 ) );
    REQUIRE( project.Run( "edit.undo" ) );
    world.Frame( 0.25 );
    CHECK( light().mRadiance.x == doctest::Approx( 5 ) );
    CHECK( not document.IsDirty() );

    // Closed by an operator, the document takes its world along.
    REQUIRE( project.Run( "document.close" ) );
    CHECK( workspace.Count() == 0 );
}

TEST_CASE( "The editor's camera turns about its focus, comes closer, and looks from a scene's camera" )
{
    EditorCamera camera;
    camera.mFocus = vec3( 1, 2, 3 );
    CHECK( glm::length( camera.Position() - camera.mFocus ) == doctest::Approx( camera.mDistance ) );

    // 200 pixels across turn it a radian; it never goes over the top.
    const f32 yaw = camera.mYaw;
    camera.Orbit( vec2( 200.0f, 0.0f ) );
    CHECK( camera.mYaw == doctest::Approx( yaw - 1.0f ) );
    camera.Orbit( vec2( 0.0f, -100000.0f ) );
    CHECK( camera.mPitch < 1.571f );
    CHECK( glm::length( camera.Position() - camera.mFocus ) == doctest::Approx( camera.mDistance ) );

    const f32 distance = camera.mDistance;
    camera.Zoom( 1.0f );
    CHECK( camera.mDistance < distance );

    // A scene's camera at (0, 5, 10) looking down -Z, as it does unturned.
    camera.LookFrom( glm::translate( mat4( 1.0f ), vec3( 0, 5, 10 ) ), 4.0f );
    CHECK( camera.mFocus.z == doctest::Approx( 6.0f ) );
    CHECK( camera.mFocus.y == doctest::Approx( 5.0f ) );
    const vec3 at = camera.Position();
    CHECK( at.x == doctest::Approx( 0.0f ).epsilon( 0.0001 ) );
    CHECK( at.y == doctest::Approx( 5.0f ) );
    CHECK( at.z == doctest::Approx( 10.0f ) );
    // And the renderer sees it from there.
    CHECK( vec3( camera.ToRenderCamera().mWorld[3] ).z == doctest::Approx( 10.0f ) );
}

namespace
{
// Where `point` is seen by `camera`: in shares of the view from its middle,
// -1 to 1 rightwards and upwards, its width `aspect` times its height.
vec2 SeenAt( const EditorCamera& camera, vec3 point, f32 aspect )
{
    const vec3 offset = point - camera.Position();
    const f32 ahead = glm::dot( offset, camera.Forward() );
    const f32 spread = tan( camera.mFov * 0.5f );
    return vec2( glm::dot( offset, camera.Right() ) / ahead / ( spread * aspect ),
                 glm::dot( offset, camera.Up() ) / ahead / spread );
}

bool Near( vec2 a, vec2 b )
{
    return glm::length( a - b ) < 1e-3f;
}
}

TEST_CASE( "The editor's camera looks about where it stands, flies, and goes round a point keeping it in its place" )
{
    EditorCamera camera;
    camera.mFocus = vec3( 1, 2, 3 );

    // Looking about, it stays where it stands.
    const vec3 eye = camera.Position();
    camera.Look( vec2( 120.0f, -40.0f ) );
    CHECK( glm::length( camera.Position() - eye ) < 1e-4f );
    CHECK( camera.mYaw == doctest::Approx( 0.0f ).epsilon( 1e-5 ) );
    CHECK( camera.mPitch == doctest::Approx( -0.25f ) );
    CHECK( glm::length( camera.Position() - camera.mFocus ) == doctest::Approx( camera.mDistance ) );

    // A second ahead at five meters a second; with shift four times as far,
    // two keys at once no farther than one; up is the world's.
    const vec3 from = camera.Position();
    camera.Fly( vec3( 0, 0, 1 ), 1.0f );
    CHECK( glm::length( camera.Position() - ( from + camera.Forward() * 5.0f ) ) < 1e-4f );
    const vec3 ahead = camera.Position();
    camera.Fly( vec3( 1, 0, 1 ), 1.0f, 4.0f );
    CHECK( glm::length( camera.Position() - ahead ) == doctest::Approx( 20.0f ) );
    const vec3 aside = camera.Position();
    camera.Fly( vec3( 0, 1, 0 ), 0.5f );
    CHECK( glm::length( camera.Position() - ( aside + vec3( 0, 2.5f, 0 ) ) ) < 1e-4f );
    camera.Faster( 1.0f );
    CHECK( camera.mFlySpeed == doctest::Approx( 5.0f / 0.85f ) );

    // Round a point off the focus: it stays where it is seen, as far.
    const vec3 pivot = camera.Position() + camera.Forward() * 6.0f + camera.Right() * 1.5f + camera.Up() * 0.5f;
    const vec2 seen = SeenAt( camera, pivot, 1.0f );
    const f32 reach = glm::length( pivot - camera.Position() );
    const f32 yaw = camera.mYaw;
    camera.OrbitAbout( vec2( 150.0f, 60.0f ), pivot );
    CHECK( camera.mYaw == doctest::Approx( yaw - 0.75f ) );
    CHECK( Near( SeenAt( camera, pivot, 1.0f ), seen ) );
    CHECK( glm::length( pivot - camera.Position() ) == doctest::Approx( reach ) );
}

TEST_CASE( "The editor's camera zooms toward what is under the mouse, and on through its focus" )
{
    EditorCamera camera;
    const f32 aspect = 1.5f;
    // What is under the mouse at a point of the view, as far as the focus.
    const vec2 at( 0.5f, -0.25f );
    const vec3 under = camera.mFocus + ( camera.Right() * ( at.x * aspect ) + camera.Up() * at.y ) *
                                           ( tan( camera.mFov * 0.5f ) * camera.mDistance );
    REQUIRE( Near( SeenAt( camera, under, aspect ), at ) );

    // Two notches in and out again: closer and back, it under the mouse all
    // along.
    const f32 distance = camera.mDistance;
    camera.Zoom( 2.0f, at, aspect );
    CHECK( camera.mDistance == doctest::Approx( distance * 0.85f * 0.85f ) );
    CHECK( Near( SeenAt( camera, under, aspect ), at ) );
    camera.Zoom( -2.0f, at, aspect );
    CHECK( camera.mDistance == doctest::Approx( distance ) );
    CHECK( Near( SeenAt( camera, under, aspect ), at ) );

    // Near the focus, the wheel goes on through it, the focus ahead.
    camera.mDistance = 0.4f;
    const vec3 focus = camera.mFocus;
    for ( int notch = 0; notch < 10; ++notch )
        camera.Zoom( 1.0f );
    CHECK( glm::dot( camera.Position() - focus, camera.Forward() ) > 1.0f );
    CHECK( camera.mDistance == doctest::Approx( 0.3f ) );
}

TEST_CASE( "Every asset made from its template imports with no error" )
{
    hmap<string, string> files{ { "project.bubble", string( cProject ) } };
    const ProjectFiles projectFiles = ProjectFiles::InMemory( files );
    const Scope<AudioDevice> audio = AudioDevice::Offline();
    auto projectMaybe = EditorProject::TryOpen(
        projectFiles, AssetRegistry::AtOnce( [projectFiles]( const AssetId& assetId, string_view kind ) {
            return projectFiles.TryRead( ImportedPath( assetId, kind ) );
        } ),
        *audio );
    REQUIRE_MESSAGE( projectMaybe, projectMaybe.error() );
    EditorProject& project = **projectMaybe;
    const vector<string_view> made{ "new.mat", "new.prefab", "new.scene", "new.anim", "new.luau" };
    for ( const string_view path : made )
        REQUIRE( project.Run( "asset.create", Args( format( R"({{ "path": "{}" }})", path ) ) ) );
    project.Refresh();
    for ( const string_view path : made )
    {
        const auto errorMaybe = project.TryErrorOf( *AssetPath::TryFrom( path ) );
        CHECK_MESSAGE( not errorMaybe, path, ":", errorMaybe ? *errorMaybe : string() );
    }
    CHECK( project.KindOf( *AssetPath::TryFrom( "new.luau" ) ) == "script" );
    CHECK( project.KindOf( *AssetPath::TryFrom( "new.scene" ) ) == "level" );
    CHECK( project.GetCatalog().TryIdOf( *AssetPath::TryFrom( "new.prefab" ) ) );
}
