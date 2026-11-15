// The engine's lit surface: PBR metallic-roughness - GGX, height-correlated
// Smith, Schlick's Fresnel - under every light of the frame, the shadowed
// directional light through its shadow map. Until materials come, every
// surface is one light grey, rough dielectric.

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
}

@group( 0 ) @binding( 0 ) var<uniform> frame: Frame;
@group( 0 ) @binding( 1 ) var<storage, read> lights: array<Light>;
@group( 0 ) @binding( 2 ) var shadow_map: texture_depth_2d;
@group( 0 ) @binding( 3 ) var shadow_sampler: sampler_comparison;
@group( 1 ) @binding( 0 ) var<storage, read> objects: array<Object>;

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
fn fs_main( in: Varyings ) -> @location( 0 ) vec4f
{
    let base_color = vec3f( 0.8 );
    let metallic = 0.0;
    let roughness = 0.6;

    let n = normalize( in.normal );
    let v = normalize( frame.camera_position - in.world );
    let n_dot_v = max( dot( n, v ), 1e-4 );
    let f0 = mix( vec3f( 0.04 ), base_color, metallic );
    let diffuse_color = base_color * ( 1.0 - metallic );

    var color = frame.ambient * diffuse_color;
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
            radiance *= light_through_shadow( in.world, n );
        }
        let h = normalize( l + v );
        let fresnel = fresnel_schlick( saturate( dot( v, h ) ), f0 );
        let specular = distribution_ggx( saturate( dot( n, h ) ), roughness ) *
                       visibility_smith( n_dot_v, n_dot_l, roughness ) * fresnel;
        let diffuse = ( 1.0 - fresnel ) * diffuse_color / pi;
        color += ( diffuse + specular ) * radiance * n_dot_l;
    }
    return vec4f( color, 1.0 );
}
