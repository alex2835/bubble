// The UI: rectangles of images and glyphs, in their canvas's pixels. A
// screen canvas over the tonemapped picture, a world canvas among the
// scene's surfaces; the same shader for both, a canvas's matrix taking its
// pixels to the clip.

struct Canvas
{
    to_clip: mat4x4f,
}

@group( 0 ) @binding( 0 ) var<storage, read> canvases: array<Canvas>;
@group( 1 ) @binding( 0 ) var image: texture_2d<f32>;
@group( 1 ) @binding( 1 ) var image_sampler: sampler;

struct Vertex
{
    @location( 0 ) position: vec2f,
    @location( 1 ) uv: vec2f,
    // In linear light.
    @location( 2 ) color: vec4f,
    @location( 3 ) canvas: u32,
    // 0 an image, 1 a glyph of a font's atlas.
    @location( 4 ) kind: u32,
    // A glyph's distance range in its atlas's pixels.
    @location( 5 ) range: f32,
}

struct Varyings
{
    @builtin( position ) clip: vec4f,
    @location( 0 ) uv: vec2f,
    @location( 1 ) color: vec4f,
    @location( 2 ) @interpolate( flat ) kind: u32,
    @location( 3 ) @interpolate( flat ) range: f32,
}

@vertex
fn vs_main( vertex: Vertex ) -> Varyings
{
    var out: Varyings;
    out.clip = canvases[vertex.canvas].to_clip * vec4f( vertex.position, 0.0, 1.0 );
    out.uv = vertex.uv;
    out.color = vertex.color;
    out.kind = vertex.kind;
    out.range = vertex.range;
    return out;
}

fn median( a: f32, b: f32, c: f32 ) -> f32
{
    return max( min( a, b ), min( max( a, b ), c ) );
}

@fragment
fn fs_main( in: Varyings ) -> @location( 0 ) vec4f
{
    // Sampled, and its derivatives taken, where every pixel of a quad is.
    let texel = textureSample( image, image_sampler, in.uv );
    let unit_range = vec2f( in.range ) / vec2f( textureDimensions( image ) );
    let screen_texels = vec2f( 1.0 ) / fwidth( in.uv );
    // How many of the screen's pixels the distance range spans here: the
    // glyph's edge a pixel wide at any size.
    let screen_range = max( 0.5 * dot( unit_range, screen_texels ), 1.0 );
    let distance = median( texel.r, texel.g, texel.b ) - 0.5;
    let coverage = clamp( distance * screen_range + 0.5, 0.0, 1.0 );
    if ( in.kind == 1u )
    {
        return vec4f( in.color.rgb, in.color.a * coverage );
    }
    return in.color * texel;
}
