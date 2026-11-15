#pragma once
#include "bubble/types/math.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"

struct SDL_Window;

namespace bubble
{
struct WindowOptions
{
    string mTitle = "bubble";
    // In the window's own coordinates, as Window::Size.
    ivec2 mSize{ 1280, 720 };
    bool mResizable = true;
};

// A window of the platform's: its size, its UI scale and, from the
// renderer's stage on, the surface drawn into. Platform::TryOpenWindow
// makes one; it closes when it goes. On the web it is the page's canvas.
class Window
{
public:
    ~Window();
    Window( Window&& other ) noexcept;
    Window& operator=( Window&& other ) noexcept;
    Window( const Window& ) = delete;
    Window& operator=( const Window& ) = delete;

    // In the window's own coordinates, the ones mouse positions are in:
    // pixels on Windows, points on a Mac and in a browser.
    ivec2 Size() const;
    // What the renderer draws.
    ivec2 PixelSize() const;
    // How many pixels the UI's unit takes: the display's scale - 150% in
    // Windows' settings - and its density - a Retina screen - together.
    f32 Scale() const;
    void SetTitle( const string& title );

private:
    friend class Platform;
    explicit Window( SDL_Window* window ) : mWindow( window ) {}

    // SDL's own - the C boundary bubble/platform keeps to itself.
    SDL_Window* mWindow = nullptr;
};
}
