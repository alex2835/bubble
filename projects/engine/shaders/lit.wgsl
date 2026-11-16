// The engine's part of every lit surface: what a material's surface() gets
// and gives, the vertex stage, and the lighting - PBR metallic-roughness:
// GGX, height-correlated Smith, Schlick's Fresnel - under every light of the
// frame, the shadowed directional light through its shadow map. The engine
// builds it after the material's own WGSL into one module, so the
// material's lines keep their numbers in an error.

struct Frame
{
    view_projection: mat4x4f,
    shadow_view_projection: mat4x4f,
    camera_position: vec3f,
    light_count: u32,
    ambient: vec3f,
    // The light the shadow map is of; light_count or past it when none is.
    shadow_light: u32,
    // How far along its normal a surface is looked up in the shadow map, in
    // the world's units: about a texel, against acne.
    shadow_normal_offset: f32,
    // The world's time in seconds.
    time: f32,
}

struct Light
{
    position: vec3f,
    range: f32,
    direction: vec3f,
    // A spot light's cone; -1 for the others.
    inner_cos: f32,
    radiance: vec3f,
    outer_cos: f32,
    // 1 for a directional light: its direction is all there is of it.
    directional: u32,
}

struct Object
{
    model: mat4x4f,
    // The inverse transpose of the model's turn and scale, for normals.
    normal: mat4x4f,
    // The model component's tint, in linear light: the surface's color is
    // multiplied by it.
    tint: vec4f,
}

@group( 0 ) @binding( 0 ) var<uniform> frame: Frame;
@group( 0 ) @binding( 1 ) var<storage, read> lights: array<Light>;
@group( 0 ) @binding( 2 ) var shadow_map: texture_depth_2d;
@group( 0 ) @binding( 3 ) var shadow_sampler: sampler_comparison;
@group( 1 ) @binding( 0 ) var<storage, read> objects: array<Object>;

// What a material's surface() is told of the point it shades.
struct SurfaceInput
{
    // In the world.
    position: vec3f,
    // In the world, of unit length; turned towards the eye on the back of a
    // two-sided surface.
    normal: vec3f,
    // From the image's top left.
    uv: vec2f,
    // From the point to the eye, of unit length.
    view: vec3f,
    // The world's time in seconds.
    time: f32,
}

// What a material's surface() says the point is.
struct SurfaceOutput
{
    // In linear light.
    albedo: vec3f,
    // 1 is opaque; it blends only in a transparent material.
    alpha: f32,
    // In the world, of unit length.
    normal: vec3f,
    metallic: f32,
    roughness: f32,
    // Light of its own, in linear light.
    emissive: vec3f,
    // How much of the light from all around reaches it.
    occlusion: f32,
}

// A white, rough dielectric with the geometry's own normal: what a
// surface() starts from.
fn surface_defaults( in: SurfaceInput ) -> SurfaceOutput
{
    var out: SurfaceOutput;
    out.albedo = vec3f( 1.0 );
    out.alpha = 1.0;
    out.normal = in.normal;
    out.metallic = 0.0;
    out.roughness = 0.5;
    out.emissive = vec3f( 0.0 );
    out.occlusion = 1.0;
    return out;
}

// The normal a normal map gives, its texel in -1..1: X along u, Y up the
// image, Z out of the surface, as glTF has it. The surface's tangents come
// from the screen's derivatives - no tangents in the vertices - so call it
// where every pixel of a quad does: not inside a branch.
fn perturb_normal( in: SurfaceInput, tangent_normal: vec3f ) -> vec3f
{
    let dp1 = dpdx( in.position );
    let dp2 = dpdy( in.position );
    let duv1 = dpdx( in.uv );
    let duv2 = dpdy( in.uv );
    let n = in.normal;
    let dp2_across = cross( dp2, n );
    let dp1_across = cross( n, dp1 );
    let tangent = dp2_across * duv1.x + dp1_across * duv2.x;
    // v grows down the image, the map's Y up it.
    let bitangent = -( dp2_across * duv1.y + dp1_across * duv2.y );
    let longest = max( dot( tangent, tangent ), dot( bitangent, bitangent ) );
    if ( longest < 1e-20 )
    {
        return n;
    }
    let scale = inverseSqrt( longest );
    return normalize( mat3x3f( tangent * scale, bitangent * scale, n ) * tangent_normal );
}

struct Vertex
{
    // The draw's first instance is its object.
    @builtin( instance_index ) object: u32,
    @location( 0 ) position: vec3f,
    @location( 1 ) normal: vec3f,
    @location( 2 ) uv: vec2f,
}

struct Varyings
{
    @builtin( position ) clip: vec4f,
    @location( 0 ) world: vec3f,
    @location( 1 ) normal: vec3f,
    @location( 2 ) uv: vec2f,
    @location( 3 ) @interpolate( flat ) object: u32,
}

@vertex
fn vs_main( vertex: Vertex ) -> Varyings
{
    let object = objects[vertex.object];
    let world = object.model * vec4f( vertex.position, 1.0 );
    var out: Varyings;
    out.clip = frame.view_projection * world;
    out.world = world.xyz;
    out.normal = ( object.normal * vec4f( vertex.normal, 0.0 ) ).xyz;
    out.uv = vertex.uv;
    out.object = vertex.object;
    return out;
}

const pi = 3.14159265;

fn distribution_ggx( n_dot_h: f32, roughness: f32 ) -> f32
{
    let a2 = roughness * roughness * roughness * roughness;
    let d = n_dot_h * n_dot_h * ( a2 - 1.0 ) + 1.0;
    return a2 / ( pi * d * d );
}

fn visibility_smith( n_dot_v: f32, n_dot_l: f32, roughness: f32 ) -> f32
{
    let a2 = roughness * roughness * roughness * roughness;
    let from_view = n_dot_l * sqrt( n_dot_v * n_dot_v * ( 1.0 - a2 ) + a2 );
    let from_light = n_dot_v * sqrt( n_dot_l * n_dot_l * ( 1.0 - a2 ) + a2 );
    return 0.5 / max( from_view + from_light, 1e-5 );
}

fn fresnel_schlick( v_dot_h: f32, f0: vec3f ) -> vec3f
{
    return f0 + ( 1.0 - f0 ) * pow( 1.0 - v_dot_h, 5.0 );
}

// The sheen the light from all around gives a surface, with no picture of
// the sky to reflect: Karis's fit of the split sum's second term. Without
// it a metal would be black wherever the sun does not glint on it.
fn ambient_specular( f0: vec3f, roughness: f32, n_dot_v: f32 ) -> vec3f
{
    let r = roughness * vec4f( -1.0, -0.0275, -0.572, 0.022 ) + vec4f( 1.0, 0.0425, 1.04, -0.04 );
    let a004 = min( r.x * r.x, exp2( -9.28 * n_dot_v ) ) * r.x + r.y;
    let scale_bias = vec2f( -1.04, 1.04 ) * a004 + r.zw;
    return f0 * scale_bias.x + scale_bias.y;
}

// How much of the shadowed light reaches `world`: 3×3 lookups, each
// filtered by the sampler. Depth is reversed - nearer the light is greater -
// so a point is lit where it is at least as near as what the map holds.
fn light_through_shadow( world: vec3f, normal: vec3f ) -> f32
{
    let clip = frame.shadow_view_projection * vec4f( world + normal * frame.shadow_normal_offset, 1.0 );
    let ndc = clip.xyz / clip.w;
    let uv = ndc.xy * vec2f( 0.5, -0.5 ) + 0.5;
    if ( any( uv < vec2f( 0.0 ) ) || any( uv > vec2f( 1.0 ) ) || ndc.z < 0.0 )
    {
        return 1.0;
    }
    let texel = 1.0 / vec2f( textureDimensions( shadow_map ) );
    var lit = 0.0;
    for ( var y = -1; y <= 1; y++ )
    {
        for ( var x = -1; x <= 1; x++ )
        {
            lit += textureSampleCompareLevel( shadow_map, shadow_sampler, uv + vec2f( f32( x ), f32( y ) ) * texel,
                                              ndc.z );
        }
    }
    return lit / 9.0;
}

@fragment
fn fs_main( in: Varyings, @builtin( front_facing ) front: bool ) -> @location( 0 ) vec4f
{
    var input: SurfaceInput;
    input.position = in.world;
    input.normal = normalize( in.normal ) * select( -1.0, 1.0, front );
    input.uv = in.uv;
    input.view = normalize( frame.camera_position - in.world );
    input.time = frame.time;
    var shaded = surface( input );
    let tint = objects[in.object].tint;
    shaded.albedo *= tint.rgb;
    shaded.alpha *= tint.a;

    let n = normalize( shaded.normal );
    let v = input.view;
    let roughness = clamp( shaded.roughness, 0.04, 1.0 );
    let n_dot_v = max( dot( n, v ), 1e-4 );
    let f0 = mix( vec3f( 0.04 ), shaded.albedo, shaded.metallic );
    let diffuse_color = shaded.albedo * ( 1.0 - shaded.metallic );

    var color = frame.ambient * ( diffuse_color + ambient_specular( f0, roughness, n_dot_v ) ) * shaded.occlusion +
                shaded.emissive;
    for ( var i = 0u; i < frame.light_count; i++ )
    {
        let light = lights[i];
        var l = -light.direction;
        var radiance = light.radiance;
        if ( light.directional == 0u )
        {
            // Inverse square, brought to 0 at the range smoothly - as Unreal
            // fades its lights.
            let to_light = light.position - in.world;
            let distance = length( to_light );
            l = to_light / max( distance, 1e-4 );
            let ratio = distance / max( light.range, 1e-4 );
            let window = saturate( 1.0 - ratio * ratio * ratio * ratio );
            radiance *= window * window / ( distance * distance + 1.0 );
            if ( light.outer_cos > -1.0 )
            {
                radiance *= smoothstep( light.outer_cos, light.inner_cos, dot( -l, light.direction ) );
            }
        }
        let n_dot_l = dot( n, l );
        if ( n_dot_l <= 0.0 )
        {
            continue;
        }
        if ( i == frame.shadow_light )
        {
            radiance *= light_through_shadow( in.world, input.normal );
        }
        let h = normalize( l + v );
        let fresnel = fresnel_schlick( saturate( dot( v, h ) ), f0 );
        let specular = distribution_ggx( saturate( dot( n, h ) ), roughness ) *
                       visibility_smith( n_dot_v, n_dot_l, roughness ) * fresnel;
        let diffuse = ( 1.0 - fresnel ) * diffuse_color / pi;
        color += ( diffuse + specular ) * radiance * n_dot_l;
    }
    return vec4f( color, shaded.alpha );
}
