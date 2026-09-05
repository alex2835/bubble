#pragma once
#include "engine/types/number.hpp"
#include "engine/types/string.hpp"
#include "engine/log/log.hpp"

// The glcall/glclear/glcheck macros and the GL error string helpers that used to
// live here are gone with OpenGL. WebGPU reports errors through the device's
// uncaptured-error callback (see GpuContext) and through pushErrorScope, so
// there is nothing to wrap individual calls in.

#define BUBBLE_ASSERT( x, str ) if( not ( x ) ) { LogError( str ); }; assert( x && str );
