#pragma once

// WebGPU-C++ is header-only, and its implementation is guarded by
// WEBGPU_CPP_IMPLEMENTATION. That macro must be defined in exactly ONE
// translation unit - see src/renderer/webgpu_impl.cpp - never in a header, or
// every TU that includes it emits the implementation and the link fails with
// duplicate symbols.
#include <webgpu/webgpu.hpp>

// RAII wrappers: wgpu::raii::Device, wgpu::raii::Buffer, ...
//
// The raw wgpu:: handle types are non-owning: HANDLE() in webgpu.hpp declares a
// class holding a WGPU* pointer with no destructor. raii::Wrapper adds refcounted
// ownership (addRef on copy, release on destruction) and pointer-like access via
// operator->. Hold these as members; do not inherit from the wgpu:: types and add
// a destructor, which produces a type with no rule-of-five that double-releases.
#include <webgpu/webgpu-raii.hpp>
