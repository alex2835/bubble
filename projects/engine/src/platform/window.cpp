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

void Window::SetCursorLocked( bool locked ) const
{
    SDL_SetWindowRelativeMouseMode( mWindow, locked );
}

NativeWindow Window::Native() const
{
    [[maybe_unused]] const SDL_PropertiesID properties = SDL_GetWindowProperties( mWindow );
#if defined( SDL_PLATFORM_WINDOWS )
    if ( void* window = SDL_GetPointerProperty( properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr ) )
        return Win32Window{ .mInstance =
                                SDL_GetPointerProperty( properties, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr ),
                            .mWindow = window };
#elif defined( SDL_PLATFORM_EMSCRIPTEN )
    const char* canvas = SDL_GetStringProperty( properties, SDL_PROP_WINDOW_EMSCRIPTEN_CANVAS_ID_STRING, "canvas" );
    return CanvasWindow{ .mSelector = "#" + string( canvas ) };
#elif defined( SDL_PLATFORM_LINUX )
    // One process runs on X11 or on Wayland, whichever SDL found.
    const string_view driver = SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "";
    if ( driver == "x11" )
        return X11Window{
            .mDisplay = SDL_GetPointerProperty( properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr ),
            .mWindow = static_cast<u64>( SDL_GetNumberProperty( properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0 ) )
        };
    if ( driver == "wayland" )
        return WaylandWindow{
            .mDisplay = SDL_GetPointerProperty( properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr ),
            .mSurface = SDL_GetPointerProperty( properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr )
        };
#endif
    return monostate{};
}
}
