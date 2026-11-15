// What the GPU tests share: a GPU - the machine's, or its software one where
// there is none - and RGBA8 pixels read as text. Not on the web: node has no
// WebGPU, and a browser answers only between frames.
#pragma once
#ifndef __EMSCRIPTEN__
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

namespace bubble
{
// The value, or the test stops with the error.
template <typename T>
T Required( expected<T, string> valueMaybe )
{
    if ( not valueMaybe )
        FAIL( valueMaybe.error() );
    return std::move( *valueMaybe );
}

inline void Required( const expected<void, string>& doneMaybe )
{
    if ( not doneMaybe )
        FAIL( doneMaybe.error() );
}

inline Scope<Gpu> StartGpu()
{
    auto gpuMaybe = Gpu::TryStartNow( {} );
    if ( not gpuMaybe )
        gpuMaybe = Gpu::TryStartNow( {}, GpuOptions{ .mFallbackAdapter = true } );
    return Required( std::move( gpuMaybe ) );
}

// The pixel at (x, y) of RGBA8 pixels `width` wide: "255 128 0 255".
inline string PixelAt( const vector<u8>& pixels, i32 width, i32 x, i32 y )
{
    const size_t at = ( static_cast<size_t>( y ) * static_cast<size_t>( width ) + static_cast<size_t>( x ) ) * 4;
    return format( "{} {} {} {}", pixels[at], pixels[at + 1], pixels[at + 2], pixels[at + 3] );
}
}
#endif
