#include "engine/pch/pch.hpp"
#include "engine/log/log.hpp"
#include "engine/renderer/framebuffer.hpp"
#include "engine/renderer/gpu_context.hpp"

namespace bubble
{
namespace
{

void ResizeAttachment( Texture2D& attachment, uvec2 size )
{
    if ( (u32)attachment.Width() != size.x or (u32)attachment.Height() != size.y )
        attachment.Resize( ivec2( size ) );
}

wgpu::RenderPassDepthStencilAttachment MakeDepthAttachment( Texture2D& depth, bool clear )
{
    wgpu::RenderPassDepthStencilAttachment attachment = wgpu::Default;
    attachment.view = depth.View();
    attachment.depthLoadOp = clear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
    attachment.depthStoreOp = wgpu::StoreOp::Store;
    attachment.depthClearValue = 1.0f;
    attachment.depthReadOnly = false;
    // Depth32Float has no stencil aspect; setting stencil ops on it is an error.
    attachment.stencilLoadOp = wgpu::LoadOp::Undefined;
    attachment.stencilStoreOp = wgpu::StoreOp::Undefined;
    attachment.stencilClearValue = 0;
    attachment.stencilReadOnly = true;
    return attachment;
}

}


Framebuffer::Framebuffer( Texture2D&& color, Texture2D&& depth )
    : mColorAttachment( std::move( color ) ),
      mDepthAttachment( std::move( depth ) )
{
    mSpecification = { mColorAttachment.Width(), mColorAttachment.Height() };
}

Framebuffer::Framebuffer( Framebuffer&& other ) noexcept
    : mColorAttachment( Texture2DSpecification::CreateRGBA8( {} ) ),
      mDepthAttachment( Texture2DSpecification::CreateDepth( {} ) )
{
    Swap( other );
}

Framebuffer& Framebuffer::operator= ( Framebuffer&& other ) noexcept
{
    if ( this != &other )
        Swap( other );
    return *this;
}

void Framebuffer::Swap( Framebuffer& other ) noexcept
{
    std::swap( mColorAttachment, other.mColorAttachment );
    std::swap( mDepthAttachment, other.mDepthAttachment );
    std::swap( mSpecification, other.mSpecification );
}

void Framebuffer::SetColorAttachment( Texture2D&& texture )
{
    mColorAttachment = std::move( texture );
}

void Framebuffer::SetDepthAttachment( Texture2D&& texture )
{
    mDepthAttachment = std::move( texture );
}

Texture2D& Framebuffer::ColorAttachment()
{
    return mColorAttachment;
}

const Texture2D& Framebuffer::ColorAttachment() const
{
    return mColorAttachment;
}

Texture2D& Framebuffer::DepthAttachment()
{
    return mDepthAttachment;
}

wgpu::raii::RenderPassEncoder Framebuffer::BeginRenderPass( wgpu::CommandEncoder encoder,
                                                            opt<vec4> clearColor,
                                                            bool clearDepth,
                                                            string_view label )
{
    wgpu::RenderPassColorAttachment colorAttachment = wgpu::Default;
    colorAttachment.view = mColorAttachment.View();
    // Required for a 2D target; leaving it zero fails validation.
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    colorAttachment.resolveTarget = nullptr;
    colorAttachment.loadOp = clearColor ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
    colorAttachment.storeOp = wgpu::StoreOp::Store;
    if ( clearColor )
        colorAttachment.clearValue = wgpu::Color{ clearColor->r, clearColor->g,
                                                  clearColor->b, clearColor->a };

    wgpu::RenderPassDepthStencilAttachment depthAttachment =
        MakeDepthAttachment( mDepthAttachment, clearDepth );

    wgpu::RenderPassDescriptor passDesc = wgpu::Default;
    passDesc.label = wgpu::StringView( label );
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAttachment;
    passDesc.depthStencilAttachment = &depthAttachment;
    passDesc.timestampWrites = nullptr;

    return wgpu::raii::RenderPassEncoder( encoder.beginRenderPass( passDesc ) );
}

wgpu::raii::RenderPassEncoder Framebuffer::BeginRenderPassUint( wgpu::CommandEncoder encoder,
                                                                opt<uvec4> clearColor,
                                                                bool clearDepth,
                                                                string_view label )
{
    wgpu::RenderPassColorAttachment colorAttachment = wgpu::Default;
    colorAttachment.view = mColorAttachment.View();
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    colorAttachment.resolveTarget = nullptr;
    colorAttachment.loadOp = clearColor ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
    colorAttachment.storeOp = wgpu::StoreOp::Store;
    if ( clearColor )
        colorAttachment.clearValue = wgpu::Color{ (f64)clearColor->r, (f64)clearColor->g,
                                                  (f64)clearColor->b, (f64)clearColor->a };

    wgpu::RenderPassDepthStencilAttachment depthAttachment =
        MakeDepthAttachment( mDepthAttachment, clearDepth );

    wgpu::RenderPassDescriptor passDesc = wgpu::Default;
    passDesc.label = wgpu::StringView( label );
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAttachment;
    passDesc.depthStencilAttachment = &depthAttachment;
    passDesc.timestampWrites = nullptr;

    return wgpu::raii::RenderPassEncoder( encoder.beginRenderPass( passDesc ) );
}

void Framebuffer::Invalidate()
{
    const uvec2 size = Size();
    ResizeAttachment( mColorAttachment, size );
    ResizeAttachment( mDepthAttachment, size );
}

i32 Framebuffer::Width() const
{
    return mSpecification.mWidth;
}

i32 Framebuffer::Height() const
{
    return mSpecification.mHeight;
}

uvec2 Framebuffer::Size() const
{
    return mSpecification.Size();
}

void Framebuffer::Resize( uvec2 size )
{
    size = glm::max( size, uvec2( 1u ) );
    if ( size == Size() )
        return;

    mSpecification.mWidth = (i32)size.x;
    mSpecification.mHeight = (i32)size.y;
    Invalidate();
}

FramebufferSpecification Framebuffer::Specification() const
{
    return mSpecification;
}

}
