#include "bubble/world/transform.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/world/world.hpp"

namespace bubble
{
namespace
{
class Transforms final : public Module
{
public:
    explicit Transforms( World& world ) : mWorld( world )
    {
        world.AddStep( Phase::Transforms, [this]( f32 ) { Update(); } );
        // Outside the tree an entity has a world transform for as long as
        // it has a transform; in it, always.
        world.OnRemoved<Transform>( [this]( Entity entity ) {
            Scene& scene = mWorld.GetScene();
            if ( not InTree( scene, entity ) )
                scene.Remove<WorldTransform>( entity );
        } );
    }

private:
    void Update()
    {
        BUBBLE_PROFILE_ZONE_NAMED( "Transforms" );
        Scene& scene = mWorld.GetScene();
        mStack.clear();
        mStack.push_back( { scene.Root(), mat4( 1.0f ) } );
        while ( not mStack.empty() )
        {
            const auto [entity, parent] = mStack.back();
            mStack.pop_back();
            const auto transformMaybe = scene.TryGet<Transform>( entity );
            const mat4 matrix = transformMaybe ? parent * LocalMatrix( *transformMaybe ) : parent;
            Set( entity, matrix );
            for ( const Entity child : ChildrenOf( scene, entity ) )
                mStack.push_back( { child, matrix } );
        }
        for ( const Entity entity : scene.EntitiesWith<Transform>() )
            if ( not InTree( scene, entity ) )
                Set( entity, LocalMatrix( scene.Get<Transform>( entity ) ) );
    }

    void Set( Entity entity, const mat4& matrix )
    {
        Scene& scene = mWorld.GetScene();
        if ( const auto worldMaybe = scene.TryGet<WorldTransform>( entity ) )
            worldMaybe->mMatrix = matrix;
        else
            scene.Add<WorldTransform>( entity, matrix );
    }

    World& mWorld;
    // Kept between frames for its room: the entities still to visit, each
    // with its parent's matrix.
    vector<pair<Entity, mat4>> mStack;
};
}

quat RotationOf( const Transform& transform )
{
    const vec3 angles = transform.mRotation;
    return glm::angleAxis( angles.y, vec3( 0.0f, 1.0f, 0.0f ) ) * glm::angleAxis( angles.x, vec3( 1.0f, 0.0f, 0.0f ) ) *
           glm::angleAxis( angles.z, vec3( 0.0f, 0.0f, 1.0f ) );
}

mat4 LocalMatrix( const Transform& transform )
{
    return glm::scale( glm::translate( mat4( 1.0f ), transform.mPosition ) * glm::mat4_cast( RotationOf( transform ) ),
                       transform.mScale );
}

ModuleType TransformModule()
{
    return ModuleType{
        .mName = "transform",
        .mDescribe =
            []( Reflection& reflection, ComponentTypes& componentTypes ) {
                reflection.Type<Transform>( "transform" )
                    .Field<&Transform::mPosition>( "position" )
                    .Field<&Transform::mRotation>( "rotation", { .mFlags = FieldInfo::Angle } )
                    .Field<&Transform::mScale>( "scale" );
                componentTypes.Add<Transform>();
            },
        .mCreate = []( World& world ) -> Scope<Module> { return CreateScope<Transforms>( world ); },
    };
}
}
