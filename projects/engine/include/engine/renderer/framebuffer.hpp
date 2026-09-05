#pragma once
#include <cstdint>
#include <cassert>
#include "engine/types/number.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/utility.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/webgpu.hpp"

namespace bubble
{

// A render pass that ends itself.
//
// Releasing a RenderPassEncoder is not the same as ending it: until end() is
// called the command encoder stays locked, and the next thing recorded on it
// fails with "encoder is locked by a previously created pass". Tying end() to
// scope exit means a pass cannot be left open by forgetting one line.
class RenderPassScope
{
public:
    explicit RenderPassScope( wgpu::raii::RenderPassEncoder pass )
        : mPass( std::move( pass ) )
    {}

    RenderPassScope( const RenderPassScope& ) = delete;
    RenderPassScope& operator=( const RenderPassScope& ) = delete;
    RenderPassScope( RenderPassScope&& ) = default;
    RenderPassScope& operator=( RenderPassScope&& ) = default;

    ~RenderPassScope() { End(); }

    void End()
    {
        if ( mPass and not mEnded )
        {
            mPass->end();
            mEnded = true;
        }
    }

    wgpu::RenderPassEncoder operator*() const { return *mPass; }
    explicit operator bool() const { return (bool)mPass; }

private:
    wgpu::raii::RenderPassEncoder mPass;
    bool mEnded = false;
};


struct FramebufferSpecification
{
    i32 mWidth = 0;
    i32 mHeight = 0;
    i32 mSamples = 1;

    uvec2 Size() const
    {
        return { mWidth, mHeight };
    }
};


// A set of render targets.
//
// There is no framebuffer object in WebGPU - attachments are named directly on
// a render pass descriptor - so this is now just the textures plus the helper
// that opens a pass against them. Bind() is gone; call BeginRenderPass instead,
// which is also where the clear-or-load decision now lives.
class Framebuffer
{
public:
    Framebuffer() = delete;
    Framebuffer( const Framebuffer& ) = delete;
    Framebuffer& operator= ( const Framebuffer& ) = delete;
    Framebuffer( Framebuffer&& other ) noexcept;
    Framebuffer& operator = ( Framebuffer&& other ) noexcept;
    Framebuffer( Texture2D&& color, Texture2D&& depth );
    ~Framebuffer() = default;

    void Swap( Framebuffer& other ) noexcept;

    void SetColorAttachment( Texture2D&& texture );
    void SetDepthAttachment( Texture2D&& texture );
    Texture2D& ColorAttachment();
    const Texture2D& ColorAttachment() const;
    Texture2D& DepthAttachment();

    // Opens a render pass onto these attachments. Pass a clear colour to clear,
    // or nullopt to load what is already there - which is how several draws land
    // in one target without wiping each other, the job glClear-or-not used to do.
    RenderPassScope BeginRenderPass( wgpu::CommandEncoder encoder,
                                     opt<vec4> clearColor,
                                     bool clearDepth,
                                     string_view label = "Render Pass" );

    // Same, for the R32Uint entity id target, whose clear value is an integer.
    RenderPassScope BeginRenderPassUint( wgpu::CommandEncoder encoder,
                                         opt<uvec4> clearColor,
                                         bool clearDepth,
                                         string_view label = "Id Pass" );

    void Invalidate();

    i32 Width() const;
    i32 Height() const;
    uvec2 Size() const;
    void Resize( uvec2 size );

    FramebufferSpecification Specification() const;

private:
    Texture2D mColorAttachment;
    Texture2D mDepthAttachment;
    FramebufferSpecification mSpecification;
};
}
