// Linear light of any brightness into what a screen shows: ACES as
// Narkowicz fits it. The target's view is sRGB and encodes the result.

@group( 0 ) @binding( 0 ) var hdr: texture_2d<f32>;

// One triangle over the whole target.
@vertex
fn vs_main( @builtin( vertex_index ) index: u32 ) -> @builtin( position ) vec4f
{
    let corner = vec2f( f32( ( index << 1u ) & 2u ), f32( index & 2u ) );
    return vec4f( corner * 2.0 - 1.0, 0.0, 1.0 );
}

fn aces( x: vec3f ) -> vec3f
{
    return saturate( ( x * ( 2.51 * x + 0.03 ) ) / ( x * ( 2.43 * x + 0.59 ) + 0.14 ) );
}

@fragment
fn fs_main( @builtin( position ) position: vec4f ) -> @location( 0 ) vec4f
{
    return vec4f( aces( textureLoad( hdr, vec2i( position.xy ), 0 ).rgb ), 1.0 );
}
