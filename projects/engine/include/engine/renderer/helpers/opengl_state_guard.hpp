#pragma once
#include <GL/glew.h>

namespace bubble
{
/// RAII class to save and restore OpenGL state
/// Usage:
///   {
///       OpenGLStateGuard guard;
///       glEnable(GL_BLEND);
///       // ... render ...
///   } // State automatically restored here

class OpenGLStateGuard
{
public:
    OpenGLStateGuard();
    ~OpenGLStateGuard();
    OpenGLStateGuard( const OpenGLStateGuard& ) = delete;
    OpenGLStateGuard& operator=( const OpenGLStateGuard& ) = delete;
    // Not movable: the members are plain GLints, so a move is a copy and both
    // objects would restore state on destruction.
    OpenGLStateGuard( OpenGLStateGuard&& ) = delete;
    OpenGLStateGuard& operator=( OpenGLStateGuard&& ) = delete;

private:
    // Enable/Disable states
    GLboolean mCullFaceEnabled;
    GLboolean mDepthTestEnabled;
    GLboolean mBlendEnabled;
    GLboolean mScissorTestEnabled;
    GLboolean mStencilTestEnabled;

    // State values
    GLint mCullFaceMode;          // GL_FRONT, GL_BACK, GL_FRONT_AND_BACK
    GLint mFrontFace;             // GL_CW, GL_CCW
    GLint mDepthFunc;             // GL_LESS, GL_LEQUAL, etc.
    GLboolean mDepthMask;         // Depth write enable/disable

    // Blend state
    GLint mBlendSrcRGB;
    GLint mBlendDstRGB;
    GLint mBlendSrcAlpha;
    GLint mBlendDstAlpha;
    GLint mBlendEquationRGB;
    GLint mBlendEquationAlpha;

    // Color mask
    GLboolean mColorMask[4];      // R, G, B, A

    // Viewport and scissor
    GLint mViewport[4];           // x, y, width, height
    GLint mScissorBox[4];         // x, y, width, height

    // Polygon mode
    GLint mPolygonMode[2];        // Front and back
};

} // namespace bubble
