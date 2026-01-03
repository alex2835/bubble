#include "engine/pch/pch.hpp"
#include "engine/renderer/renderer.hpp"

#include <GL/glew.h>

namespace bubble
{
Renderer::Renderer()
{
    glDisable( GL_BLEND );
    glEnable( GL_DEPTH_TEST );
    glEnable( GL_CULL_FACE );
    //glDisable( GL_CULL_FACE );

    // Uniform buffers
    VertexBufferLayout vertexUniformVertexBufferLayout{
        { "uProjection", GLSLDataType::Mat4  },
        { "uView", GLSLDataType::Mat4  }
    };
    mVertexUniformBuffer = CreateRef<UniformBuffer>( 0, 
                                                     "VertexUniformBuffer",
                                                     std::move( vertexUniformVertexBufferLayout ) );

    VertexBufferLayout lightsInfoUniformVertexBufferLayout{
        { "uNumLights", GLSLDataType::Int },
        { "uViewPos", GLSLDataType::Float3 }
    };
    mLightsInfoUniformBuffer = CreateRef<UniformBuffer>( 1,
                                                         "LightsInfoUniformBuffer",
                                                         std::move( lightsInfoUniformVertexBufferLayout ) );
    
    VertexBufferLayout lightsUniformVertexBufferLayout{
        { "type", GLSLDataType::Int },
        { "brightness", GLSLDataType::Float },
        { "constant", GLSLDataType::Float },
        { "linear", GLSLDataType::Float },
        { "quadratic", GLSLDataType::Float },
        { "cutOff", GLSLDataType::Float },
        { "outerCutOff", GLSLDataType::Float },
        { "color", GLSLDataType::Float3 },
        { "direction", GLSLDataType::Float3 },
        { "position", GLSLDataType::Float3 },
    };
    constexpr int nLights = 128;
    mLightsUniformBuffer = CreateRef<UniformBuffer>( 2,
                                                     "LightsUniformBuffer",
                                                     std::move( lightsUniformVertexBufferLayout ),
                                                     nLights );
}


void Renderer::SetCameraUniformBuffers( const Camera& camera, const Framebuffer& framebuffer )
{
    auto framebufferSize = framebuffer.Size();
    auto projectionMat = camera.GetPprojectionMat( framebufferSize.x, framebufferSize.y );
    auto lookAtMat = camera.GetLookatMat();

    auto vertexBufferElement = mVertexUniformBuffer->Element( 0 );
    vertexBufferElement.SetMat4( "uProjection", projectionMat );
    vertexBufferElement.SetMat4( "uView", lookAtMat );
}


void Renderer::SetLightsUniformBuffer( const Camera& camera, const std::vector<Light>& lights )
{
    // Lights info: camera pos and num lights
    auto fragmentBufferElement = mLightsInfoUniformBuffer->Element( 0 );
    fragmentBufferElement.SetInt( "uNumLights", (i32)lights.size() );
    fragmentBufferElement.SetFloat3( "uViewPos", camera.mPosition );

    // Lights
    for ( i32 i = 0; i < lights.size(); i++ )
    {
        const auto& light = lights[i];
        auto lightBufferElement = mLightsUniformBuffer->Element( i );
        lightBufferElement.SetInt( "type", (i32)light.mType );
        lightBufferElement.SetFloat( "brightness", light.mBrightness );
        lightBufferElement.SetFloat( "constant", light.mConstant );
        lightBufferElement.SetFloat( "linear", light.mLinear );
        lightBufferElement.SetFloat( "quadratic", light.mQuadratic );
        lightBufferElement.SetFloat( "cutOff", glm::cos( glm::radians( light.mCutOff ) ) );
        lightBufferElement.SetFloat( "outerCutOff", glm::cos( glm::radians( light.mOuterCutOff ) ) );
        lightBufferElement.SetFloat3( "color", light.mColor );
        lightBufferElement.SetFloat3( "direction", light.mDirection );
        lightBufferElement.SetFloat3( "position", light.mPosition );
    }
}


void Renderer::ClearScreen( vec4 color )
{
    glClearColor( color.r, color.g, color.b, color.a );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

}


void Renderer::ClearScreenUint( uvec4 color )
{
    glClearBufferuiv( GL_COLOR, 0, glm::value_ptr( color ) );
    glClear( GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
}

void Renderer::DrawMesh( const Mesh& mesh, const Ref<Shader>& shader, DrawingPrimitive drawingPrimitive )
{
    mesh.BindVertexArray();
    if ( shader->mModules.test( ShaderModule::Material ) )
        mesh.ApplyMaterial( shader );
    glDrawElements( (GLenum)drawingPrimitive, (i32)mesh.IndiciesSize(), GL_UNSIGNED_INT, nullptr );
}



void Renderer::DrawMesh( const Mesh& mesh,
                         const Ref<Shader>& shader,
                         const mat4& transform,
                         DrawingPrimitive drawingPrimitive )
{
    if ( not shader )
        return;

    // Set uniform buffers
    shader->SetUniformBuffer( mVertexUniformBuffer );
    if ( shader->mModules.test( ShaderModule::Light ) )
    {
        shader->SetUniformBuffer( mLightsInfoUniformBuffer );
        shader->SetUniformBuffer( mLightsUniformBuffer );
    }

    // Draw model
    shader->SetUniMat4( "uModel", transform );
    DrawMesh( mesh, shader, drawingPrimitive );
}


void Renderer::DrawModel( const Ref<Model>& model,
                          const mat4& transform,
                          const Ref<Shader>& shader,
                          DrawingPrimitive drawingPrimitive )
{
    if ( not model or not shader )
        return;

    // Set uniform buffers
    shader->SetUniformBuffer( mVertexUniformBuffer );
    if ( shader->mModules.test( ShaderModule::Light ) )
    {
        shader->SetUniformBuffer( mLightsInfoUniformBuffer );
        shader->SetUniformBuffer( mLightsUniformBuffer );
    }

    // Draw model
    shader->SetUniMat4( "uModel", transform );
    for ( const auto& mesh : model->mMeshes )
        Renderer::DrawMesh( mesh, shader, drawingPrimitive );
}

}