module;
#include <SDL3/SDL.h>

namespace hitagi::interop {
inline constexpr auto sdl_init_video                          = SDL_INIT_VIDEO;
inline constexpr auto sdl_prop_window_wayland_display_pointer = SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER;
inline constexpr auto sdl_prop_window_wayland_surface_pointer = SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER;
inline constexpr auto sdl_prop_window_win32_hwnd_pointer      = SDL_PROP_WINDOW_WIN32_HWND_POINTER;
inline constexpr auto sdl_window_hidden                       = SDL_WINDOW_HIDDEN;
inline constexpr auto sdl_window_high_pixel_density           = SDL_WINDOW_HIGH_PIXEL_DENSITY;
inline constexpr auto sdl_window_maximized                    = SDL_WINDOW_MAXIMIZED;
inline constexpr auto sdl_window_resizable                    = SDL_WINDOW_RESIZABLE;
inline constexpr auto sdl_window_vulkan                       = SDL_WINDOW_VULKAN;
}  // namespace hitagi::interop
#undef SDL_INIT_VIDEO
#undef SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER
#undef SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER
#undef SDL_PROP_WINDOW_WIN32_HWND_POINTER
#undef SDL_WINDOW_HIDDEN
#undef SDL_WINDOW_HIGH_PIXEL_DENSITY
#undef SDL_WINDOW_MAXIMIZED
#undef SDL_WINDOW_RESIZABLE
#undef SDL_WINDOW_VULKAN

export module interop.sdl;

export {
    using ::SDL_CreateSystemCursor;
    using ::SDL_CreateWindow;
    using ::SDL_Cursor;
    using ::SDL_DestroyWindow;
    using ::SDL_Event;
    using ::SDL_EVENT_QUIT;
    using ::SDL_EVENT_WINDOW_MAXIMIZED;
    using ::SDL_EVENT_WINDOW_MINIMIZED;
    using ::SDL_EVENT_WINDOW_RESIZED;
    using ::SDL_EVENT_WINDOW_RESTORED;
    using ::SDL_GetCurrentVideoDriver;
    using ::SDL_GetError;
    using ::SDL_GetPointerProperty;
    using ::SDL_GetWindowFlags;
    using ::SDL_GetWindowPosition;
    using ::SDL_GetWindowProperties;
    using ::SDL_GetWindowSize;
    using ::SDL_GetWindowSizeInPixels;
    using ::SDL_Init;
    using ::SDL_PollEvent;
    using ::SDL_SetCursor;
    using ::SDL_SetWindowSize;
    using ::SDL_SetWindowTitle;
    using ::SDL_strcmp;
    using ::SDL_SYSTEM_CURSOR_DEFAULT;
    using ::SDL_SYSTEM_CURSOR_EW_RESIZE;
    using ::SDL_SYSTEM_CURSOR_MOVE;
    using ::SDL_SYSTEM_CURSOR_NESW_RESIZE;
    using ::SDL_SYSTEM_CURSOR_NOT_ALLOWED;
    using ::SDL_SYSTEM_CURSOR_NS_RESIZE;
    using ::SDL_SYSTEM_CURSOR_NWSE_RESIZE;
    using ::SDL_SYSTEM_CURSOR_POINTER;
    using ::SDL_SYSTEM_CURSOR_TEXT;
    using ::SDL_WarpMouseInWindow;
    using ::SDL_Window;
    inline constexpr auto SDL_INIT_VIDEO                          = hitagi::interop::sdl_init_video;
    inline constexpr auto SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER = hitagi::interop::sdl_prop_window_wayland_display_pointer;
    inline constexpr auto SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER = hitagi::interop::sdl_prop_window_wayland_surface_pointer;
    inline constexpr auto SDL_PROP_WINDOW_WIN32_HWND_POINTER      = hitagi::interop::sdl_prop_window_win32_hwnd_pointer;
    inline constexpr auto SDL_WINDOW_HIDDEN                       = hitagi::interop::sdl_window_hidden;
    inline constexpr auto SDL_WINDOW_HIGH_PIXEL_DENSITY           = hitagi::interop::sdl_window_high_pixel_density;
    inline constexpr auto SDL_WINDOW_MAXIMIZED                    = hitagi::interop::sdl_window_maximized;
    inline constexpr auto SDL_WINDOW_RESIZABLE                    = hitagi::interop::sdl_window_resizable;
    inline constexpr auto SDL_WINDOW_VULKAN                       = hitagi::interop::sdl_window_vulkan;
}
