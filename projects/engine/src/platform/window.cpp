#include "bubble/platform/window.hpp"
#include "bubble/types/utility.hpp"
#include <SDL3/SDL.h>

namespace bubble
{
Window::~Window()
{
    if ( mWindow != nullptr )
        SDL_DestroyWindow( mWindow );
}

Window::Window( Window&& other ) noexcept : mWindow( exchange( other.mWindow, nullptr ) )
{
}

Window& Window::operator=( Window&& other ) noexcept
{
    if ( this != &other )
    {
        if ( mWindow != nullptr )
            SDL_DestroyWindow( mWindow );
        mWindow = exchange( other.mWindow, nullptr );
    }
    return *this;
}

ivec2 Window::Size() const
{
    ivec2 size{ 0 };
    SDL_GetWindowSize( mWindow, &size.x, &size.y );
    return size;
}

ivec2 Window::PixelSize() const
{
    ivec2 size{ 0 };
    SDL_GetWindowSizeInPixels( mWindow, &size.x, &size.y );
    return size;
}

f32 Window::Scale() const
{
    const f32 scale = SDL_GetWindowDisplayScale( mWindow );
    // 0 is SDL saying it could not tell.
    return scale > 0.0f ? scale : 1.0f;
}

void Window::SetTitle( const string& title )
{
    SDL_SetWindowTitle( mWindow, title.c_str() );
}
}
