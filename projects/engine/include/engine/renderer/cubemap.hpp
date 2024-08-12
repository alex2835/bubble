#pragma once
#include "engine/utils/types.hpp"
#include "engine/renderer/texture.hpp"

namespace bubble
{
class Cubemap
{
public:
    Cubemap() = default;
    Cubemap( i32 width, i32 height, const Texture2DSpecification& spec );
    // right, left, top, bottom, front, back
    Cubemap( const std::array<Scope<u8[]>, 6>& data, const Texture2DSpecification& spec );
    Cubemap( const Cubemap& ) = delete;
    Cubemap& operator=( const Cubemap& ) = delete;
    Cubemap( Cubemap&& ) noexcept;
    Cubemap& operator=( Cubemap&& ) noexcept;
    ~Cubemap();

    void Swap( Cubemap& other ) noexcept;

    void Bind( i32 slot = 0 );

private:
    GLuint mRendererID = 0;
};

}