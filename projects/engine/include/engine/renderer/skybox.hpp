#pragma once
#include "glm/gtx/transform.hpp"
#include "engine/renderer/cubemap.hpp"
#include "engine/renderer/buffer.hpp"


namespace bubble
{
struct Skybox
{
    Cubemap mSkybox;

    Skybox() = default;
    Skybox( Cubemap&& skybox );

    Skybox( const Skybox& ) = delete;
    Skybox& operator=( const Skybox& ) = delete;

    Skybox( Skybox&& ) = default;
    Skybox& operator=( Skybox&& ) = default;

    void Bind( const Ref<VertexArray>& vertex_array, i32 slot = 0 );

    // Generate matrix for correct skybox rendering
    static mat4 GetViewMatrix( mat4 view, f32 rotation = 0 );
};
}