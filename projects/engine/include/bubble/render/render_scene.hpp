#pragma once
#include "bubble/assets/asset_registry.hpp"
#include "bubble/assets/material_asset.hpp"
#include "bubble/assets/model_asset.hpp"
#include "bubble/core/color.hpp"
#include "bubble/scene/entity.hpp"
#include "bubble/types/matrix.hpp"
#include "bubble/world/module.hpp"

namespace bubble
{
// What a world is seen through. It looks down its entity's -Z, +Y up; its
// field of view is vertical, in radians, edited as degrees.
struct Camera
{
    f32 mFov = 1.0471976f; // 60°
    f32 mNear = 0.1f;
    f32 mFar = 1000.0f;
};

// A light as far as the sun, shining down its entity's -Z.
struct DirectionalLight
{
    Color mColor;
    f32 mIntensity = 2.0f;
    bool mShadows = true;
};

// A light at its entity, fading out by `range`.
struct PointLight
{
    Color mColor;
    f32 mIntensity = 10.0f;
    f32 mRange = 10.0f;
};

// A point light in a cone down its entity's -Z: full inside `inner_angle`
// of the axis, fading out by `outer_angle`; radians, edited as degrees.
struct SpotLight
{
    Color mColor;
    f32 mIntensity = 10.0f;
    f32 mRange = 10.0f;
    f32 mInnerAngle = 0.5235988f; // 30°
    f32 mOuterAngle = 0.6981317f; // 40°
};

// A model drawn where its entity is: each of its meshes with the material
// in its slot - a mesh past the list, or with none, in the standard one -
// its color times `tint`, which changes this one alone and keeps it drawn
// with the others that share its material.
struct Model
{
    AssetId mAsset;
    vector<AssetId> mMaterials;
    Color mTint;
};

enum class RenderLightKind : u8
{
    Directional,
    Point,
    Spot,
};

// A light as the renderer takes it: in the world, in linear light.
struct RenderLight
{
    RenderLightKind mKind = RenderLightKind::Directional;
    vec3 mPosition{ 0.0f };
    // Where it shines; a point light's is unused.
    vec3 mDirection{ 0.0f, 0.0f, -1.0f };
    // Its color in linear light, times its intensity.
    vec3 mRadiance{ 0.0f };
    f32 mRange = 0;
    // The cosines of a spot light's angles: 1 and -1 for the others, so
    // every light is a spot light the shader need not tell apart.
    f32 mInnerCos = -1;
    f32 mOuterCos = -1;
    bool mShadows = false;
};

struct RenderCamera
{
    // From the camera's space to the world's.
    mat4 mWorld{ 1.0f };
    f32 mFov = 1.0471976f;
    f32 mNear = 0.1f;
    f32 mFar = 1000.0f;
};

// One model to draw. It may be loading still: the renderer draws what has
// come.
struct RenderItem
{
    Entity mEntity;
    AssetRef<ModelAsset> mModel;
    // By mesh; an empty ref is the standard material.
    vector<AssetRef<MaterialAsset>> mMaterials;
    mat4 mWorld{ 1.0f };
    // In linear light.
    vec4 mTint{ 1.0f };
};

// What there is to draw in a world, as the render module gathers it once a
// frame (Phase::Render): no entities to walk, no components - Bevy's
// extract, Unreal's render proxies. A world's service; the renderer draws
// it with one of its cameras, or with the editor's own.
struct RenderScene
{
    vector<RenderItem> mItems;
    vector<RenderLight> mLights;
    // In the order their components lie; the game looks through the first.
    vector<RenderCamera> mCameras;
    // The world's time in seconds, its fixed steps' and the part of the
    // next one: what a material moves by.
    f32 mTime = 0;
};

// The camera, light and model components, and the RenderScene gathered
// from them; models come from `assets`, which outlives every world.
ModuleType RenderModule( AssetRegistry& assets );
}
