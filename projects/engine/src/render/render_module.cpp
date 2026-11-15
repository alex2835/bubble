#include "bubble/core/profile.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"

namespace bubble
{
namespace
{
// A Model's asset, loading or loaded: built from the component, never saved.
struct ModelRuntime
{
    AssetRef<ModelAsset> mModel;
};

class Render final : public Module
{
public:
    Render( World& world, AssetRegistry& assets ) : mWorld( world ), mAssets( assets )
    {
        world.OnAdded<Model>( [this]( Entity entity ) { Build( entity ); } );
        world.OnChanged<Model>( "asset"_n, [this]( Entity entity ) { Build( entity ); } );
        world.OnRemoved<Model>( [this]( Entity entity ) { mWorld.GetScene().Remove<ModelRuntime>( entity ); } );
        world.AddStep( Phase::Render, [this]( f32 ) { Gather(); } );
        world.GetServices().Add( mScene );
    }

private:
    void Build( Entity entity )
    {
        Scene& scene = mWorld.GetScene();
        const AssetId& assetId = scene.Get<Model>( entity ).mAsset;
        // None chosen yet - a model just added in the editor - draws nothing.
        scene.Add<ModelRuntime>( entity, assetId ? mAssets.Load<ModelAsset>( assetId ) : AssetRef<ModelAsset>() );
    }

    mat4 WorldOf( Entity entity ) const
    {
        const auto worldMaybe = mWorld.GetScene().TryGet<WorldTransform>( entity );
        return worldMaybe ? worldMaybe->mMatrix : mat4( 1.0f );
    }

    static vec3 Radiance( const Color& color, f32 intensity ) { return vec3( ToLinear( color ) ) * intensity; }

    static vec3 Forward( const mat4& world )
    {
        return glm::normalize( vec3( world * vec4( 0.0f, 0.0f, -1.0f, 0.0f ) ) );
    }

    void Gather()
    {
        BUBBLE_PROFILE_ZONE_NAMED( "Render: gather" );
        Scene& scene = mWorld.GetScene();
        mScene.mItems.clear();
        mScene.mLights.clear();
        mScene.mCameras.clear();
        scene.ForEach<ModelRuntime>( [&]( Entity entity, const ModelRuntime& runtime ) {
            mScene.mItems.push_back(
                RenderItem{ .mEntity = entity, .mModel = runtime.mModel, .mWorld = WorldOf( entity ) } );
        } );
        scene.ForEach<DirectionalLight>( [&]( Entity entity, const DirectionalLight& light ) {
            const mat4 world = WorldOf( entity );
            mScene.mLights.push_back( RenderLight{ .mKind = RenderLightKind::Directional,
                                                   .mPosition = vec3( world[3] ),
                                                   .mDirection = Forward( world ),
                                                   .mRadiance = Radiance( light.mColor, light.mIntensity ),
                                                   .mRange = 0,
                                                   .mInnerCos = -1,
                                                   .mOuterCos = -1,
                                                   .mShadows = light.mShadows } );
        } );
        scene.ForEach<PointLight>( [&]( Entity entity, const PointLight& light ) {
            const mat4 world = WorldOf( entity );
            mScene.mLights.push_back( RenderLight{ .mKind = RenderLightKind::Point,
                                                   .mPosition = vec3( world[3] ),
                                                   .mDirection = Forward( world ),
                                                   .mRadiance = Radiance( light.mColor, light.mIntensity ),
                                                   .mRange = light.mRange,
                                                   .mInnerCos = -1,
                                                   .mOuterCos = -1,
                                                   .mShadows = false } );
        } );
        scene.ForEach<SpotLight>( [&]( Entity entity, const SpotLight& light ) {
            const mat4 world = WorldOf( entity );
            mScene.mLights.push_back( RenderLight{ .mKind = RenderLightKind::Spot,
                                                   .mPosition = vec3( world[3] ),
                                                   .mDirection = Forward( world ),
                                                   .mRadiance = Radiance( light.mColor, light.mIntensity ),
                                                   .mRange = light.mRange,
                                                   .mInnerCos = cos( light.mInnerAngle ),
                                                   .mOuterCos = cos( light.mOuterAngle ),
                                                   .mShadows = false } );
        } );
        scene.ForEach<Camera>( [&]( Entity entity, const Camera& camera ) {
            mScene.mCameras.push_back( RenderCamera{
                .mWorld = WorldOf( entity ), .mFov = camera.mFov, .mNear = camera.mNear, .mFar = camera.mFar } );
        } );
    }

    World& mWorld;
    AssetRegistry& mAssets;
    RenderScene mScene;
};
}

ModuleType RenderModule( AssetRegistry& assets )
{
    return ModuleType{
        .mName = "render",
        .mDescribe =
            []( Reflection& reflection, ComponentTypes& componentTypes ) {
                const FieldInfo color{ .mFlags = FieldInfo::Color };
                const FieldInfo angle{ .mMin = 0, .mMax = 90, .mFlags = FieldInfo::Angle };
                reflection.Type<Camera>( "camera" )
                    .Field<&Camera::mFov>( "fov", { .mMin = 1, .mMax = 179, .mFlags = FieldInfo::Angle } )
                    .Field<&Camera::mNear>( "near" )
                    .Field<&Camera::mFar>( "far" );
                reflection.Type<DirectionalLight>( "directional_light" )
                    .Field<&DirectionalLight::mColor>( "color", color )
                    .Field<&DirectionalLight::mIntensity>( "intensity" )
                    .Field<&DirectionalLight::mShadows>( "shadows" );
                reflection.Type<PointLight>( "point_light" )
                    .Field<&PointLight::mColor>( "color", color )
                    .Field<&PointLight::mIntensity>( "intensity" )
                    .Field<&PointLight::mRange>( "range" );
                reflection.Type<SpotLight>( "spot_light" )
                    .Field<&SpotLight::mColor>( "color", color )
                    .Field<&SpotLight::mIntensity>( "intensity" )
                    .Field<&SpotLight::mRange>( "range" )
                    .Field<&SpotLight::mInnerAngle>( "inner_angle", angle )
                    .Field<&SpotLight::mOuterAngle>( "outer_angle", angle );
                reflection.Type<Model>( "model" ).Field<&Model::mAsset>( "asset" );
                componentTypes.Add<Camera>();
                componentTypes.Add<DirectionalLight>();
                componentTypes.Add<PointLight>();
                componentTypes.Add<SpotLight>();
                componentTypes.Add<Model>();
            },
        .mCreate = [&assets]( World& world ) -> Scope<Module> { return CreateScope<Render>( world, assets ); },
    };
}
}
