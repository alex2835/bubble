#include "engine/pch/pch.hpp"
#include <GL/glew.h>
#include "engine/renderer/renderer.hpp"

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
        { "Type", GLSLDataType::Int },
        { "Brightness", GLSLDataType::Float },
        { "Constant", GLSLDataType::Float },    
        { "Linear", GLSLDataType::Float },
        { "Quadratic", GLSLDataType::Float },
        { "CutOff", GLSLDataType::Float },
        { "OuterCutOff", GLSLDataType::Float },
        { "Color", GLSLDataType::Float3 },
        { "Direction", GLSLDataType::Float3 },
        { "Position", GLSLDataType::Float3 },
    };
    int nLights = 30;
    mLightsUniformBuffer = CreateRef<UniformBuffer>( 2,
                                                     "LightUniformBuffer",
                                                     std::move( lightsUniformVertexBufferLayout ),
                                                     nLights );
}


void Renderer::SetUniformBuffers( const Camera& camera, const Framebuffer& framebuffer )
{
    auto framebufferSize = framebuffer.Size();
    auto projectionMat = camera.GetPprojectionMat( framebufferSize.x, framebufferSize.y );
    auto lookAtMat = camera.GetLookatMat();

    auto vertexBufferElement = mVertexUniformBuffer->Element( 0 );
    vertexBufferElement.SetMat4( "uProjection", projectionMat );
    vertexBufferElement.SetMat4( "uView", lookAtMat );

    auto fragmentBufferElement = mLightsInfoUniformBuffer->Element( 0 );
    fragmentBufferElement.SetInt( "uNumLights", 0 );
    fragmentBufferElement.SetFloat3( "uViewPos", camera.mPosition );
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
        shader->SetUniformBuffer( mLightsInfoUniformBuffer );

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
        shader->SetUniformBuffer( mLightsInfoUniformBuffer );

    // Draw model
    shader->SetUniMat4( "uModel", transform );
    for ( const auto& mesh : model->mMeshes )
        Renderer::DrawMesh( mesh, shader, drawingPrimitive );
}

}