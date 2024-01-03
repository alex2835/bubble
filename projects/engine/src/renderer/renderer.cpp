#include "engine/renderer/renderer.hpp"

namespace bubble
{

void Renderer::DrawMesh( const Mesh& mesh )
{
    mesh.BindVertetxArray();
    //mesh.mMaterial.Set( shader );
    glDrawElements( GL_TRIANGLES, (uint32_t)mesh.IndiciesSize(), GL_UNSIGNED_INT, nullptr );
}

void Renderer::DrawModel( const Ref<Model>& model )
{
    model->mShader->Bind();
    for ( const auto& mesh : model->mMeshes )
        Renderer::DrawMesh( mesh );
}

}