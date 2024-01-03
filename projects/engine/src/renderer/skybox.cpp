
#include "engine/utils/pointers.hpp"
#include "engine/renderer/skybox.hpp"

namespace bubble
{
Skybox::Skybox( Cubemap&& skybox )
    : mSkybox( std::move( skybox ) )
{
}

void Skybox::Bind( const Ref<VertexArray>& vertex_array, int slot )
{
    vertex_array->Bind();
    mSkybox.Bind( slot );
}

glm::mat4 Skybox::GetViewMatrix( glm::mat4 view, float rotation )
{
    view = glm::rotate( view, rotation, glm::vec3( 0, 1, 0 ) );
    view = glm::mat4( glm::mat3( view ) );
    return view;
}

}