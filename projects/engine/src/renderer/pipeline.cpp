#include "engine/pch/pch.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "engine/renderer/pipeline.hpp"
#include "engine/renderer/gpu_context.hpp"
#include "engine/log/log.hpp"

namespace bubble
{
namespace
{

wgpu::PrimitiveTopology ToWGPU( DrawingPrimitive primitive )
{
    switch ( primitive )
    {
        case DrawingPrimitive::Triangles: return wgpu::PrimitiveTopology::TriangleList;
        case DrawingPrimitive::Lines:     return wgpu::PrimitiveTopology::LineList;
        case DrawingPrimitive::Points:    return wgpu::PrimitiveTopology::PointList;
    }
    BUBBLE_ASSERT( false, "Unknown DrawingPrimitive" );
    return wgpu::PrimitiveTopology::TriangleList;
}

u64 AlignTo( u64 value, u64 alignment )
{
    return ( value + alignment - 1 ) & ~( alignment - 1 );
}

}


void DrawUniforms::SetNormalMatrix( const mat3& m )
{
    for ( i32 column = 0; column < 3; column++ )
        mNormalMatrix[column] = vec4( m[column], 0.0f );
}

u32 VertexLayoutAttributeMask( const VertexLayout& layout )
{
    u32 mask = 0;
    for ( const auto& slot : layout.Slots() )
        mask |= 1u << (u32)slot.mSemantic;
    return mask;
}


// ---------------------------------------------------------------------------
// StandardLayouts
// ---------------------------------------------------------------------------

StandardLayouts::StandardLayouts()
{
    // Zero initialized rather than wgpu::Default.
    //
    // BindGroupLayoutEntry::setDefault() sets the sub-layouts it is not using to
    // Undefined, but Undefined is 1, not 0 - the current header distinguishes it
    // from BindingNotUsed, which is 0 - and setDefault also leaves
    // texture.viewDimension at 2D. wgpu-native then resolves a buffer binding as
    // a texture one, and createBindGroup fails with a type mismatch. Zeroing
    // leaves every sub-layout at BindingNotUsed, so only the one filled in below
    // is live.
    const auto uniformEntry = []( u32 binding, wgpu::ShaderStage visibility,
                                  bool dynamic, u64 minSize )
    {
        wgpu::BindGroupLayoutEntry entry = {};
        entry.binding = binding;
        entry.visibility = visibility;
        entry.buffer.type = wgpu::BufferBindingType::Uniform;
        entry.buffer.hasDynamicOffset = dynamic;
        entry.buffer.minBindingSize = minSize;
        return entry;
    };

    // Frame: camera, then the two light blocks.
    {
        array<wgpu::BindGroupLayoutEntry, 3> entries = {
            uniformEntry( 0, wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment, false, 0 ),
            uniformEntry( 1, wgpu::ShaderStage::Fragment, false, 0 ),
            uniformEntry( 2, wgpu::ShaderStage::Fragment, false, 0 ),
        };

        wgpu::BindGroupLayoutDescriptor desc = wgpu::Default;
        desc.label = wgpu::StringView( "Frame Bind Group Layout" );
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        mFrame = wgpu::raii::BindGroupLayout( Gpu().Device().createBindGroupLayout( desc ) );
    }

    // Material: parameters, three maps and one sampler shared by them.
    {
        array<wgpu::BindGroupLayoutEntry, 5> entries = {};
        entries[0] = uniformEntry( 0, wgpu::ShaderStage::Fragment, false, 0 );
        for ( u32 i = 1; i <= 3; i++ )
        {
            entries[i] = {};
            entries[i].binding = i;
            entries[i].visibility = wgpu::ShaderStage::Fragment;
            entries[i].texture.sampleType = wgpu::TextureSampleType::Float;
            entries[i].texture.viewDimension = wgpu::TextureViewDimension::_2D;
            entries[i].texture.multisampled = false;
        }
        entries[4] = {};
        entries[4].binding = 4;
        entries[4].visibility = wgpu::ShaderStage::Fragment;
        entries[4].sampler.type = wgpu::SamplerBindingType::Filtering;

        wgpu::BindGroupLayoutDescriptor desc = wgpu::Default;
        desc.label = wgpu::StringView( "Material Bind Group Layout" );
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        mMaterial = wgpu::raii::BindGroupLayout( Gpu().Device().createBindGroupLayout( desc ) );
    }

    // Draw: one dynamically offset slot per draw.
    {
        wgpu::BindGroupLayoutEntry entry =
            uniformEntry( 0, wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
                          true, sizeof( DrawUniforms ) );

        wgpu::BindGroupLayoutDescriptor desc = wgpu::Default;
        desc.label = wgpu::StringView( "Draw Bind Group Layout" );
        desc.entryCount = 1;
        desc.entries = &entry;
        mDraw = wgpu::raii::BindGroupLayout( Gpu().Device().createBindGroupLayout( desc ) );
    }

    array<WGPUBindGroupLayout, cBindGroupCount> layouts = {
        (WGPUBindGroupLayout)*mFrame,
        (WGPUBindGroupLayout)*mMaterial,
        (WGPUBindGroupLayout)*mDraw,
    };

    wgpu::PipelineLayoutDescriptor desc = wgpu::Default;
    desc.label = wgpu::StringView( "Standard Pipeline Layout" );
    desc.bindGroupLayoutCount = layouts.size();
    desc.bindGroupLayouts = layouts.data();
    mPipelineLayout = wgpu::raii::PipelineLayout( Gpu().Device().createPipelineLayout( desc ) );
}


// ---------------------------------------------------------------------------
// DrawUniformRing
// ---------------------------------------------------------------------------

DrawUniformRing::DrawUniformRing()
{
    wgpu::Limits limits = wgpu::Default;
    u64 alignment = 256;
    if ( Gpu().Device().getLimits( &limits ) == wgpu::Status::Success and
         limits.minUniformBufferOffsetAlignment > 0 )
        alignment = limits.minUniformBufferOffsetAlignment;

    mSlotSize = AlignTo( sizeof( DrawUniforms ), alignment );
    Reallocate( 256 );
}

void DrawUniformRing::Reallocate( u64 slotCount )
{
    mSlotCount = slotCount;
    mStaging.assign( mSlotSize * mSlotCount, 0 );

    wgpu::BufferDescriptor bufferDesc = wgpu::Default;
    bufferDesc.label = wgpu::StringView( "Draw Uniform Ring" );
    bufferDesc.size = mSlotSize * mSlotCount;
    bufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    bufferDesc.mappedAtCreation = false;
    mBuffer = wgpu::raii::Buffer( Gpu().Device().createBuffer( bufferDesc ) );
    if ( not mBuffer )
        LogError( "Draw uniform ring: buffer of {} bytes failed", bufferDesc.size );

    wgpu::BindGroupEntry entry = {};
    entry.binding = 0;
    entry.buffer = *mBuffer;
    entry.offset = 0;
    // The bound range is one slot; the dynamic offset selects which.
    entry.size = sizeof( DrawUniforms );

    wgpu::BindGroupDescriptor desc = wgpu::Default;
    desc.label = wgpu::StringView( "Draw Bind Group" );
    desc.layout = Gpu().Layouts().Draw();
    desc.entryCount = 1;
    desc.entries = &entry;
    mBindGroup = wgpu::raii::BindGroup( Gpu().Device().createBindGroup( desc ) );
}

void DrawUniformRing::Reset()
{
    mUsed = 0;
}

u32 DrawUniformRing::Push( const DrawUniforms& uniforms )
{
    if ( mUsed >= mSlotCount )
    {
        // Growing mid frame would orphan the bind group the already-recorded
        // commands point at, so the extra room is taken on the next frame
        // instead and this draw reuses the last slot.
        LogWarning( "Draw uniform ring exhausted at {} slots, growing next frame", mSlotCount );
        const u64 wanted = mSlotCount * 2;
        mPendingGrowth = std::max( mPendingGrowth, wanted );
        return (u32)( ( mSlotCount - 1 ) * mSlotSize );
    }

    const u64 offset = mUsed * mSlotSize;
    std::memcpy( mStaging.data() + offset, &uniforms, sizeof( DrawUniforms ) );
    mUsed++;
    return (u32)offset;
}

void DrawUniformRing::Flush()
{
    if ( mUsed > 0 )
        Gpu().Queue().writeBuffer( *mBuffer, 0, mStaging.data(), mUsed * mSlotSize );

    if ( mPendingGrowth > mSlotCount )
    {
        Reallocate( mPendingGrowth );
        mPendingGrowth = 0;
    }
}


// ---------------------------------------------------------------------------
// Pipelines
// ---------------------------------------------------------------------------

wgpu::raii::RenderPipeline CreateRenderPipeline( wgpu::ShaderModule module,
                                                 const PipelineKey& key,
                                                 const VertexLayout& vertexLayout,
                                                 string_view label )
{
    // Kept alive until createRenderPipeline returns - the descriptor only holds
    // pointers into it.
    VertexLayoutDescriptors vertexDescriptors = BuildVertexLayoutDescriptors( vertexLayout );

    wgpu::BlendState blendState = wgpu::Default;
    blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = wgpu::BlendOperation::Add;
    blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blendState.alpha.dstFactor = wgpu::BlendFactor::One;
    blendState.alpha.operation = wgpu::BlendOperation::Add;

    wgpu::ColorTargetState colorTarget = wgpu::Default;
    colorTarget.format = key.mColorFormat;
    // Blending stays off by default, as it was in the GL renderer. The
    // billboards rely on discard for their cutout, not on alpha blending.
    colorTarget.blend = key.mBlend ? &blendState : nullptr;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragmentState = wgpu::Default;
    fragmentState.module = module;
    fragmentState.entryPoint = wgpu::StringView( "fs_main" );
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    wgpu::DepthStencilState depthStencil = wgpu::Default;
    depthStencil.format = key.mDepthFormat;
    depthStencil.depthWriteEnabled = wgpu::OptionalBool::True;
    depthStencil.depthCompare = wgpu::CompareFunction::Less;
    depthStencil.stencilReadMask = 0;
    depthStencil.stencilWriteMask = 0;

    wgpu::RenderPipelineDescriptor desc = wgpu::Default;
    desc.label = wgpu::StringView( label );
    desc.layout = Gpu().Layouts().Pipeline();
    desc.vertex.module = module;
    desc.vertex.entryPoint = wgpu::StringView( "vs_main" );
    desc.vertex.constantCount = 0;
    desc.vertex.constants = nullptr;
    desc.vertex.bufferCount = vertexDescriptors.mLayouts.size();
    desc.vertex.buffers = vertexDescriptors.mLayouts.data();
    desc.primitive.topology = ToWGPU( key.mPrimitive );
    desc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    desc.primitive.frontFace = wgpu::FrontFace::CCW;
    desc.primitive.cullMode = key.mCullBackFaces ? wgpu::CullMode::Back : wgpu::CullMode::None;
    desc.fragment = &fragmentState;
    desc.depthStencil = key.mDepthFormat == wgpu::TextureFormat::Undefined ? nullptr : &depthStencil;
    desc.multisample.count = 1;
    desc.multisample.mask = ~0u;
    desc.multisample.alphaToCoverageEnabled = false;

    return wgpu::raii::RenderPipeline( Gpu().Device().createRenderPipeline( desc ) );
}

}
