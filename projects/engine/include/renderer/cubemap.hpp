#pragma once
#include <string>
#include "utils/imexp.hpp"
#include "renderer/texture.hpp"

namespace bubble
{
class BUBBLE_ENGINE_EXPORT Cubemap
{
public:
    Cubemap() = default;
    Cubemap( int width, int height, const Texture2DSpecification& spec );
    // Open skybox (files in dir: right, left, top, bottom, front, back |.jpg, .png, ...|)
    Cubemap( const std::string& dir, const std::string& ext = ".jpg", const Texture2DSpecification& spec = {} );
    // right, left, top, bottom, front, back
    Cubemap( uint8_t* const data[6], const Texture2DSpecification& spec );
    ~Cubemap();

    Cubemap( const Cubemap& ) = delete;
    Cubemap& operator=( const Cubemap& ) = delete;

    Cubemap( Cubemap&& ) noexcept;
    Cubemap& operator=( Cubemap&& ) noexcept;

    void Bind( int slot = 0 );

private:
    GLuint mRendererID = 0;
};

}