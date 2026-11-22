#include "bubble/editor/editor_icons.hpp"
#include "bubble/animation/animation.hpp"
#include "bubble/audio/audio.hpp"
#include "bubble/physics/physics.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/prefab_link.hpp"
#include "bubble/scripts/script_module.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/ui/ui.hpp"

namespace bubble
{
string_view IconOf( const Scene& scene, Entity entity )
{
    if ( scene.Has<PrefabInstance>( entity ) )
        return Icon::cPrefab;
    if ( const auto markerMaybe = TryMarkerOf( scene, entity ) )
        return *markerMaybe;
    if ( scene.Has<Camera>( entity ) )
        return Icon::cVideo;
    if ( scene.Has<CharacterController>( entity ) )
        return Icon::cPerson;
    // The UI's kinds before its box, which each of them has.
    if ( scene.Has<UiCanvas>( entity ) )
        return Icon::cCanvas;
    if ( scene.Has<UiButton>( entity ) )
        return Icon::cButton;
    if ( scene.Has<UiToggle>( entity ) )
        return Icon::cToggle;
    if ( scene.Has<UiSlider>( entity ) )
        return Icon::cSlider;
    if ( scene.Has<UiText>( entity ) )
        return Icon::cText;
    if ( scene.Has<UiImage>( entity ) )
        return Icon::cImage;
    if ( scene.Has<UiRect>( entity ) )
        return Icon::cRect;
    if ( scene.Has<Animator>( entity ) )
        return Icon::cAnimator;
    if ( scene.Has<Model>( entity ) )
        return Icon::cBox;
    if ( scene.Has<AudioSource>( entity ) )
        return Icon::cVolume;
    if ( scene.Has<RigidBody>( entity ) or scene.Has<BoxCollider>( entity ) or scene.Has<SphereCollider>( entity ) or
         scene.Has<CapsuleCollider>( entity ) or scene.Has<MeshCollider>( entity ) )
        return Icon::cCollider;
    if ( scene.Has<Script>( entity ) )
        return Icon::cScript;
    if ( scene.Has<BoneAttachment>( entity ) )
        return Icon::cBone;
    if ( not ChildrenOf( scene, entity ).empty() )
        return Icon::cFolder;
    return Icon::cEmpty;
}

opt<string_view> TryMarkerOf( const Scene& scene, Entity entity )
{
    // A model shows where it is by itself, whatever else it carries.
    if ( scene.Has<Model>( entity ) )
        return nullopt;
    if ( scene.Has<DirectionalLight>( entity ) )
        return Icon::cSun;
    if ( scene.Has<SpotLight>( entity ) )
        return Icon::cFlashlight;
    if ( scene.Has<PointLight>( entity ) )
        return Icon::cLightbulb;
    if ( scene.Has<Camera>( entity ) )
        return Icon::cVideo;
    if ( scene.Has<AudioSource>( entity ) )
        return Icon::cVolume;
    if ( scene.Has<AudioListener>( entity ) )
        return Icon::cEar;
    return nullopt;
}

vector<Entity> MarkedEntities( const Scene& scene )
{
    vector<Entity> marked;
    const auto add = [&]( const auto& entities ) {
        for ( const Entity entity : entities )
            if ( not ranges::contains( marked, entity ) and TryMarkerOf( scene, entity ) )
                marked.push_back( entity );
    };
    add( scene.EntitiesWith<DirectionalLight>() );
    add( scene.EntitiesWith<SpotLight>() );
    add( scene.EntitiesWith<PointLight>() );
    add( scene.EntitiesWith<Camera>() );
    add( scene.EntitiesWith<AudioSource>() );
    add( scene.EntitiesWith<AudioListener>() );
    return marked;
}
}
