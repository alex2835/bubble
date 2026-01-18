#include "engine/pch/pch.hpp"
#include "engine/renderer/helpers/opengl_state_guard.hpp"

namespace bubble
{
OpenGLStateGuard::OpenGLStateGuard()
{
    // Query enable/disable states
    mCullFaceEnabled = glIsEnabled(GL_CULL_FACE);
    mDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    mBlendEnabled = glIsEnabled(GL_BLEND);
    mScissorTestEnabled = glIsEnabled(GL_SCISSOR_TEST);
    mStencilTestEnabled = glIsEnabled(GL_STENCIL_TEST);

    // Query cull face state
    glGetIntegerv(GL_CULL_FACE_MODE, &mCullFaceMode);
    glGetIntegerv(GL_FRONT_FACE, &mFrontFace);

    // Query depth state
    glGetIntegerv(GL_DEPTH_FUNC, &mDepthFunc);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &mDepthMask);

    // Query blend state
    glGetIntegerv(GL_BLEND_SRC_RGB, &mBlendSrcRGB);
    glGetIntegerv(GL_BLEND_DST_RGB, &mBlendDstRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &mBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &mBlendDstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &mBlendEquationRGB);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &mBlendEquationAlpha);

    // Query color mask
    glGetBooleanv(GL_COLOR_WRITEMASK, mColorMask);

    // Query viewport and scissor
    glGetIntegerv(GL_VIEWPORT, mViewport);
    glGetIntegerv(GL_SCISSOR_BOX, mScissorBox);

    // Query polygon mode
    glGetIntegerv(GL_POLYGON_MODE, mPolygonMode);
}

OpenGLStateGuard::~OpenGLStateGuard()
{
    // Restore enable/disable states
    if (mCullFaceEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);

    if (mDepthTestEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (mBlendEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (mScissorTestEnabled)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    if (mStencilTestEnabled)
        glEnable(GL_STENCIL_TEST);
    else
        glDisable(GL_STENCIL_TEST);

    // Restore cull face state
    glCullFace(mCullFaceMode);
    glFrontFace(mFrontFace);

    // Restore depth state
    glDepthFunc(mDepthFunc);
    glDepthMask(mDepthMask);

    // Restore blend state
    glBlendFuncSeparate(mBlendSrcRGB, mBlendDstRGB, mBlendSrcAlpha, mBlendDstAlpha);
    glBlendEquationSeparate(mBlendEquationRGB, mBlendEquationAlpha);

    // Restore color mask
    glColorMask(mColorMask[0], mColorMask[1], mColorMask[2], mColorMask[3]);

    // Restore viewport and scissor
    glViewport(mViewport[0], mViewport[1], mViewport[2], mViewport[3]);
    glScissor(mScissorBox[0], mScissorBox[1], mScissorBox[2], mScissorBox[3]);

    // Restore polygon mode
    glPolygonMode(GL_FRONT_AND_BACK, mPolygonMode[0]);
}

} // namespace bubble
