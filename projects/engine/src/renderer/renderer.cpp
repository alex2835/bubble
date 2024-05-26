#include "engine/renderer/renderer.hpp"

namespace bubble
{
Renderer::Renderer()
{
    glDisable( GL_BLEND );
    glEnable( GL_DEPTH_TEST );
    glEnable( GL_CULL_FACE );

    // Uniform buffers
    BufferLayout vertexUniformBufferLayout{
        { "uProjection", GLSLDataType::Mat4  },
        { "uView", GLSLDataType::Mat4  }
    };
    mVertexUniformBuffer = CreateRef<UniformBuffer>( 0, 
                                                     "VertexUniformBuffer",
                                                     std::move( vertexUniformBufferLayout ) );

    BufferLayout lightsInfoUniformBufferLayout{
        { "uNumLights", GLSLDataType::Int },
        { "uViewPos", GLSLDataType::Float3 }
    };
    mLightsInfoUniformBuffer = CreateRef<UniformBuffer>( 1,
                                                       "LightsInfoUniformBuffer",
                                                       std::move( lightsInfoUniformBufferLayout ) );
    
    BufferLayout lightsUniformBufferLayout{
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
                                                     std::move( lightsUniformBufferLayout ),
                                                     nLights );
}

void Renderer::ClearScreen( vec4 color )
{
    glClearColor( color.r, color.g, color.b, color.a );
    glClear( GL_COLOR_BUFFER_BIT | 
             GL_DEPTH_BUFFER_BIT |
             GL_STENCIL_BUFFER_BIT );

}

void Renderer::ClearScreenUint( uvec4 color )
{
    glClearBufferuiv( GL_COLOR, 0, glm::value_ptr( color ) );
    glClear( GL_DEPTH_BUFFER_BIT );
    glClear( GL_DEPTH_BUFFER_BIT |
             GL_STENCIL_BUFFER_BIT );

}


void Renderer::DrawMesh( const Mesh& mesh, 
                         const Ref<Shader>& shader,
                         const mat4& transform )
{
    mesh.BindVertetxArray();
    mesh.ApplyMaterial( shader );
    glDrawElements( GL_TRIANGLES, (GLsizei)mesh.IndiciesSize(), GL_UNSIGNED_INT, nullptr );
}

void Renderer::DrawModel( const Ref<Model>& model, 
                          const mat4& transform,
                          const Ref<Shader>& shader )
{
    shader->SetUniformBuffer( mVertexUniformBuffer );
    shader->SetUniformBuffer( mLightsInfoUniformBuffer );
    shader->SetUniMat4( "uModel", transform );
    for ( const auto& mesh : model->mMeshes )
        Renderer::DrawMesh( mesh, shader, transform );
}


}