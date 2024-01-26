#include "engine/renderer/renderer.hpp"

namespace bubble
{
void Renderer::ClearScreen( vec4 color )
{
    glClearColor( color.r, color.g, color.b, color.a );
    glClear( GL_COLOR_BUFFER_BIT | 
             GL_DEPTH_BUFFER_BIT |
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

void Renderer::DrawModel( const Ref<Model>& model, const mat4& transform )
{
    model->mShader->SetUniformBuffer( mVertexUniformBuffer );
    model->mShader->SetUniformBuffer( mFragmentUniformBuffer );
    model->mShader->SetUniMat4( "uModel", transform );
    for ( const auto& mesh : model->mMeshes )
        Renderer::DrawMesh( mesh, model->mShader, transform);
}


Renderer::Renderer()
{
    BufferLayout vertexUniformBufferLayout{
        { "uProjection", GLSLDataType::Mat4  },
        { "uView", GLSLDataType::Mat4  }
    };
    mVertexUniformBuffer = CreateRef<UniformBuffer>( "VertexUniformBuffer",
                                                     0, 
                                                     std::move( vertexUniformBufferLayout ) );

    BufferLayout fragmentUniformBufferLayout{
        { "uViewPos", GLSLDataType::Float3 }
    };
    mFragmentUniformBuffer = CreateRef<UniformBuffer>( "FragmentUniformBuffer", 
                                                       1,
                                                       std::move( fragmentUniformBufferLayout ) );


    //BufferLayout layout{
    //    { "Type", GLSLDataType::Int },
    //    { "Brightness", GLSLDataType::Float },
    //    { "Constant", GLSLDataType::Float },
    //    { "Linear", GLSLDataType::Float },
    //    { "Quadratic", GLSLDataType::Float },
    //    { "CutOff", GLSLDataType::Float },
    //    { "OuterCutOff", GLSLDataType::Float },
    //    { "Color", GLSLDataType::Float3 },
    //    { "Direction", GLSLDataType::Float3 },
    //    { "Position", GLSLDataType::Float3 },
    //};
    //int nLights = 30;
    //int reserved_data = 16; // for nLights
    //mUBOLights = CreateRef<UniformBuffer>( 1, layout, nLights, reserved_data );
}

}