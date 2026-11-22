// The editor's icons: what an entity is shown as in the scene's tree, what
// the viewport marks, where the marks fall on the picture and what a click
// on them takes.
#include "bubble/audio/audio.hpp"
#include "bubble/editor/editor_icons.hpp"
#include "bubble/editor/viewport.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/prefab_link.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/ui/ui.hpp"
#include "bubble/world/transform.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}
}

TEST_CASE( "An entity's icon in the tree says what it is, the most telling of its components first" )
{
    Scene scene;
    const auto child = [&]( Entity parent, string_view name ) {
        const Entity entity = CreateChild( scene, parent, Make( name ) );
        scene.Add<Transform>( entity, Transform{} );
        return entity;
    };
    const Entity sun = child( scene.Root(), "sun" );
    scene.Add<DirectionalLight>( sun );
    const Entity lamp = child( scene.Root(), "lamp" );
    scene.Add<PointLight>( lamp );
    const Entity crate = child( scene.Root(), "crate" );
    scene.Add<Model>( crate );
    // A model with a light on it is a model to look at.
    const Entity bulb = child( scene.Root(), "bulb" );
    scene.Add<Model>( bulb );
    scene.Add<PointLight>( bulb );
    const Entity button = child( scene.Root(), "button" );
    scene.Add<UiRect>( button );
    scene.Add<UiButton>( button );
    const Entity group = child( scene.Root(), "group" );
    const Entity empty = child( group, "empty" );
    const Entity player = child( scene.Root(), "player" );
    scene.Add<PrefabInstance>( player );
    scene.Add<Camera>( player );

    CHECK( IconOf( scene, sun ) == Icon::cSun );
    CHECK( IconOf( scene, lamp ) == Icon::cLightbulb );
    CHECK( IconOf( scene, crate ) == Icon::cBox );
    CHECK( IconOf( scene, bulb ) == Icon::cBox );
    CHECK( IconOf( scene, button ) == Icon::cButton );
    CHECK( IconOf( scene, group ) == Icon::cFolder );
    CHECK( IconOf( scene, empty ) == Icon::cEmpty );
    CHECK( IconOf( scene, player ) == Icon::cPrefab );

    // The viewport marks what the picture does not show: not the bulb.
    CHECK( TryMarkerOf( scene, lamp ) == Icon::cLightbulb );
    CHECK( TryMarkerOf( scene, bulb ) == nullopt );
    CHECK( TryMarkerOf( scene, crate ) == nullopt );
    const vector<Entity> marked = MarkedEntities( scene );
    CHECK( marked.size() == 3 );
}

TEST_CASE( "The viewport's marks fall where their entities are seen, the nearest drawn last and clicked first" )
{
    Scene scene;
    const auto light = [&]( string_view name, vec3 position ) {
        const Entity entity = CreateChild( scene, scene.Root(), Make( name ) );
        scene.Add<Transform>( entity, Transform{ .mPosition = position } );
        scene.Add<PointLight>( entity );
        return entity;
    };
    const Entity far = light( "far", vec3( 0, 0, -20 ) );
    const Entity near = light( "near", vec3( 0, 0, -5 ) );
    const Entity behind = light( "behind", vec3( 0, 0, 5 ) );
    const Entity aside = light( "aside", vec3( 100, 0, -5 ) );
    // Looking down -z from the origin onto a picture 200 by 100.
    const vec2 size( 200, 100 );
    const mat4 viewProjection = glm::perspective( glm::radians( 60.0f ), 2.0f, 0.1f, 100.0f );

    const vector<Marker> markers = MarkersOf( scene, viewProjection, size );
    REQUIRE( markers.size() == 2 );
    CHECK( markers[0].mEntity == far );
    CHECK( markers[1].mEntity == near );
    CHECK( markers[1].mAt.x == doctest::Approx( 100.0f ) );
    CHECK( markers[1].mAt.y == doctest::Approx( 50.0f ) );
    CHECK( ranges::none_of(
        markers, [&]( const Marker& marker ) { return marker.mEntity == behind or marker.mEntity == aside; } ) );

    // Both under the middle; the near one is over the far one.
    CHECK( TryMarkerAt( markers, vec2( 103, 52 ), 12.0f ) == near );
    CHECK( TryMarkerAt( markers, vec2( 113, 50 ), 12.0f ) == nullopt );
}
