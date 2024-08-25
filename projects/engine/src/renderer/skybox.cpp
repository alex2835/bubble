
#include "engine/renderer/skybox.hpp"

namespace bubble
{
Skybox::Skybox( Cubemap&& skybox )
    : mSkybox( std::move( skybox ) )
{
}

void Skybox::Bind( const Ref<VertexArray>& vertex_array, i32 slot )
{
    vertex_array->Bind();
    mSkybox.Bind( slot );
}

mat4 Skybox::GetViewMatrix( mat4 view, f32 rotation )
{
    view = rotate( view, rotation, vec3( 0, 1, 0 ) );
    view = mat4( mat3( view ) );
    return view;
}

}