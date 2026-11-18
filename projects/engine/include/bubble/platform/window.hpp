#pragma once
#include "bubble/types/math.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/variant.hpp"

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

// What a window is to the system, for the GPU to draw into: the system's
// own handles, passed from bubble/platform to bubble/render/gpu as they are -
// the one place they leave either.
struct Win32Window
{
    void* mInstance = nullptr;
    void* mWindow = nullptr;
};

struct X11Window
{
    void* mDisplay = nullptr;
    u64 mWindow = 0;
};

struct WaylandWindow
{
    void* mDisplay = nullptr;
    void* mSurface = nullptr;
};

// A canvas of the page, by its CSS selector: "#canvas".
struct CanvasWindow
{
    string mSelector;
};

// Nothing to draw into: SDL's dummy driver, a system not supported yet.
using NativeWindow = variant<monostate, Win32Window, X11Window, WaylandWindow, CanvasWindow>;

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
    // Held: the cursor hidden and kept in the window, the mouse's moves
    // still coming - a camera the mouse turns. A browser holds it only after
    // a click on the page.
    void SetCursorLocked( bool locked ) const;
    // Taking text: TextInput events come as keys are typed - and a phone
    // shows its keyboard - until it is switched off.
    void SetTextInput( bool on ) const;
    NativeWindow Native() const;

private:
    friend class Platform;
    explicit Window( SDL_Window* window ) : mWindow( window ) {}

    // SDL's own - the C boundary bubble/platform keeps to itself.
    SDL_Window* mWindow = nullptr;
};
}
