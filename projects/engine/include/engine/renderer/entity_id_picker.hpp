#pragma once
#include "engine/types/number.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/array.hpp"
#include "engine/renderer/webgpu.hpp"
#include "engine/renderer/framebuffer.hpp"

namespace bubble
{

// Reads a rectangle out of the entity id buffer, asynchronously.
//
// glReadPixels stalled until the GPU drained and handed the answer back in the
// same call. WebGPU has no synchronous readback at all: the texture is copied
// into a buffer, the buffer is mapped, and the result arrives a frame or two
// later through a callback.
//
// That is less of a change to the editor than it sounds. A click's selection
// already did not take effect until the following frame - the gizmo reads
// mSelection before the click is processed, and hotkeys read it at the top of
// the next frame - so resolving one frame later matches what already happened.
//
// Lifecycle: Request, then CaptureFrom once the id pass has been submitted,
// then TakeResult on a later frame. Cancel invalidates anything in flight; the
// viewport must call it when it resizes or leaves editing mode, because the
// attachment the copy reads from is recreated on resize.
class EntityIdPicker
{
public:
    EntityIdPicker() = default;

    // Screen space rectangle in the id buffer, origin at the bottom left. A
    // single pixel is start == end.
    void Request( uvec2 start, uvec2 end );

    // True while a request is waiting for its id pass to be drawn. The editor
    // uses this to skip rendering entity ids on frames where nobody asked -
    // that pass is a second full traversal of the scene.
    bool WantsIdPass() const { return mState == State::Requested; }

    bool Busy() const { return mState != State::Idle; }

    // Copies the requested rectangle out of the id attachment. Call after the
    // id pass has been submitted, in the same frame.
    void CaptureFrom( Framebuffer& framebuffer );

    // Non blocking. Returns true once, when the result has arrived.
    bool TakeResult( vector<u32>& outPixels, uvec2& outSize );

    // Drops anything in flight. Any result still on its way is ignored.
    void Cancel();

private:
    // Called from the buffer map callback.
    void OnMapped();

    enum class State
    {
        Idle,
        Requested,  // waiting for the id pass
        InFlight,   // copied, waiting on the map callback
        Ready,
    };

    State mState = State::Idle;
    uvec2 mStart = uvec2( 0u );
    uvec2 mEnd = uvec2( 0u );
    uvec2 mSize = uvec2( 0u );
    u64 mBytesPerRow = 0;
    u64 mBufferSize = 0;
    // Bumped by Cancel so a callback for an abandoned request is discarded
    // rather than being taken for the current one.
    u64 mGeneration = 0;

    wgpu::raii::Buffer mReadback;
    vector<u32> mPixels;
};

}
