// The single translation unit that emits the WebGPU-C++ implementation.
//
// webgpu.hpp is header-only: declarations are always visible, but the function
// bodies compile only where WEBGPU_CPP_IMPLEMENTATION is defined. It must be
// defined exactly once across the whole program, so it lives here and nowhere
// else. Do not add anything to this file.
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>
