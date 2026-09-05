#pragma once
#include "engine/types/number.hpp"
#include "engine/types/map.hpp"
#include "engine/renderer/webgpu.hpp"
#include "engine/renderer/buffer.hpp"
#include "engine/renderer/texture.hpp"

namespace bubble
{

// Bind group indices, matching @group(N) in the WGSL.
//
// Grouped by how often they change: the frame group is set once per pass, the
// material group once per mesh, and the draw group per draw through a dynamic
// offset into one ring buffer.
enum class BindGroupIndex : u32
{
    Frame    = 0,
    Material = 1,
    Draw     = 2,
};
constexpr u32 cBindGroupCount = 3;


// Per draw uniforms. Laid out for the WGSL uniform address space: a mat3x3 is
// three 16 byte columns, so it occupies 48 bytes rather than 36.
struct DrawUniforms
{
    mat4 mModel = mat4( 1.0f );
    // mat3 columns padded to vec4 each.
    vec4 mNormalMatrix[3] = { vec4( 1, 0, 0, 0 ), vec4( 0, 1, 0, 0 ), vec4( 0, 0, 1, 0 ) };
    // Billboards. These were loose uniforms; they are per draw like everything
    // else here, so they ride along rather than needing a group of their own.
    vec4 mBillboardPos = vec4( 0.0f );
    vec4 mBillboardSize = vec4( 1.0f );
    vec4 mTintColor = vec4( 1.0f );
    u32 mObjectId = 0;
    u32 mPad0 = 0;
    u32 mPad1 = 0;
    u32 mPad2 = 0;

    void SetNormalMatrix( const mat3& m );
};
static_assert( sizeof( DrawUniforms ) == 176, "DrawUniforms must match the WGSL struct" );


// What a draw needs beyond the shader itself. Everything here was mutable
// global state in OpenGL; in WebGPU it is baked into the pipeline, so each
// distinct combination is a separate object.
enum class DrawingPrimitive
{
    Triangles,
    Lines,
    Points
};

struct PipelineKey
{
    wgpu::TextureFormat mColorFormat = wgpu::TextureFormat::Undefined;
    wgpu::TextureFormat mDepthFormat = wgpu::TextureFormat::Undefined;
    DrawingPrimitive mPrimitive = DrawingPrimitive::Triangles;
    bool mCullBackFaces = true;
    bool mBlend = false;
    // Which vertex attributes the mesh actually provides. Part of the key
    // because the pipeline declares them, so a mesh without tangents needs a
    // different pipeline from one with them.
    u32 mAttributeMask = 0;

    bool operator==( const PipelineKey& ) const = default;
};

u32 VertexLayoutAttributeMask( const VertexLayout& layout );


// The bind group layouts every pipeline shares, and the pipeline layout built
// from them. Created once with the device.
//
// They are fixed rather than reflected from each shader so that a bind group
// built once - the frame group, say - works with every pipeline. A shader that
// does not use a binding simply ignores it; a layout may declare more than the
// shader reads.
class StandardLayouts
{
public:
    StandardLayouts();

    wgpu::BindGroupLayout Frame() const { return *mFrame; }
    wgpu::BindGroupLayout Material() const { return *mMaterial; }
    wgpu::BindGroupLayout Draw() const { return *mDraw; }
    wgpu::PipelineLayout Pipeline() const { return *mPipelineLayout; }

private:
    wgpu::raii::BindGroupLayout mFrame;
    wgpu::raii::BindGroupLayout mMaterial;
    wgpu::raii::BindGroupLayout mDraw;
    wgpu::raii::PipelineLayout mPipelineLayout;
};


// A growable uniform buffer handed out one aligned slot per draw.
//
// WebGPU has no push constants (wgpu-native has an extension, but the web
// backend does not, so it is off limits), and a bind group per draw would be
// wasteful. Instead every draw writes its uniforms into a slot of one buffer
// and the draw bind group is set with that slot's dynamic offset.
class DrawUniformRing
{
public:
    DrawUniformRing();

    // Call once at the top of a frame.
    void Reset();

    // Stages one draw's uniforms and returns the dynamic offset for them.
    u32 Push( const DrawUniforms& uniforms );

    // Uploads everything staged this frame. Must happen before the commands
    // that reference it are submitted.
    void Flush();

    wgpu::BindGroup GetBindGroup() const { return *mBindGroup; }

private:
    void Reallocate( u64 slotCount );

    wgpu::raii::Buffer mBuffer;
    wgpu::raii::BindGroup mBindGroup;
    vector<u8> mStaging;
    u64 mSlotSize = 0;   // sizeof(DrawUniforms) rounded up to the offset alignment
    u64 mSlotCount = 0;
    u64 mUsed = 0;
    // Growth is deferred to Flush: reallocating mid frame would orphan the bind
    // group that already-recorded draw commands point at.
    u64 mPendingGrowth = 0;
};


wgpu::raii::RenderPipeline CreateRenderPipeline( wgpu::ShaderModule module,
                                                 const PipelineKey& key,
                                                 const VertexLayout& vertexLayout,
                                                 string_view label );

}
