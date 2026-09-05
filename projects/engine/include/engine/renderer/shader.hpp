#pragma once
#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_containers.hpp>
#include "engine/utils/error.hpp"
#include "engine/types/number.hpp"
#include "engine/types/pointer.hpp"
#include "engine/types/string.hpp"
#include "engine/types/map.hpp"
#include "engine/types/array.hpp"
#include "engine/utils/filesystem.hpp"
#include "engine/renderer/webgpu.hpp"
#include "engine/renderer/buffer.hpp"
#include "engine/renderer/pipeline.hpp"

namespace bubble
{
using UniformDescription = map<string, GLSLDataType>;

// The value a uniform holds when the shader declares no explicit one.
//
// Under OpenGL this was read back off the linked program with glGetUniformfv,
// which recovered whatever the GLSL initializer said. WGSL does not allow an
// initializer on a uniform at all and exposes no reflection, so the value now
// comes from a `// @default(...)` annotation parsed out of the source.
struct UniformDefault
{
    array<f32, 16> mFloats{};
    array<i32, 4> mInts{};
};
using UniformDefaults = map<string, UniformDefault>;

// Where each of a shader's own uniforms sits in its UserUniforms block.
//
// Under OpenGL a uniform was addressed by name and the driver did the packing.
// Writing into a buffer means knowing the byte offset, which is what the WGSL
// reflection in the shader loader works out.
using UniformOffsets = map<string, u32>;

enum class ShaderModule
{
    None,
    Material,
    Light,
};
using ShaderModules = magic_enum::containers::bitset<ShaderModule>;


// A compiled WGSL module plus the pipelines built from it.
//
// This used to be a GL program with a uniform location cache. Both halves
// changed: WGSL compiles one module containing both entry points, and uniforms
// are no longer set by name - they go into the bind groups described in
// pipeline.hpp. What is left here is the module, the reflected description of
// the shader's own uniforms, and a cache of pipeline variants.
class Shader
{
public:
    Shader() = default;

    Shader( const Shader& ) = delete;
    Shader& operator= ( const Shader& ) = delete;

    Shader( Shader&& ) noexcept;
    Shader& operator= ( Shader&& ) noexcept;
    ~Shader() = default;

    void Swap( Shader& other ) noexcept;

    bool Valid() const { return (bool)mModule; }
    wgpu::ShaderModule GetModule() const { return *mModule; }

    // The pipeline for this target, topology and vertex layout, created on
    // first use. Every combination is a separate immutable object, which is
    // what replaced glEnable/glBlendFunc/glDepthFunc and friends.
    wgpu::RenderPipeline GetPipeline( const PipelineKey& key, const VertexLayout& layout );

    // Drops every cached pipeline. Needed after a hot reload swaps in a new
    // module, since the old pipelines still reference the old one.
    void ClearPipelines();

public:
    string mName;
    path mPath;
    ShaderModules mModules;
    wgpu::raii::ShaderModule mModule;
    UniformDescription mUniformDescriptors;
    UniformDefaults mUniformDefaults;
    UniformOffsets mUniformOffsets;
    // Size of the shader's UserUniforms block, zero when it declares none.
    u32 mUserUniformSize = 0;

private:
    // Linear search: a shader has a handful of variants at most, so a map would
    // cost more than it saves.
    vector<pair<PipelineKey, wgpu::raii::RenderPipeline>> mPipelines;
};

}
