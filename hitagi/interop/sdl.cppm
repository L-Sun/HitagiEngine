module;
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#ifdef __APPLE__
#include <SDL3/SDL_metal.h>
#endif

export module interop.sdl;

namespace hitagi::interop {
inline constexpr auto sdl_init_video                = SDL_INIT_VIDEO;
inline constexpr auto sdl_window_hidden             = SDL_WINDOW_HIDDEN;
inline constexpr auto sdl_window_high_pixel_density = SDL_WINDOW_HIGH_PIXEL_DENSITY;
inline constexpr auto sdl_window_maximized          = SDL_WINDOW_MAXIMIZED;
inline constexpr auto sdl_window_resizable          = SDL_WINDOW_RESIZABLE;
inline constexpr auto sdl_window_vulkan             = SDL_WINDOW_VULKAN;
inline constexpr auto sdl_window_metal              = SDL_WINDOW_METAL;
}  // namespace hitagi::interop
#undef SDL_INIT_VIDEO
#undef SDL_WINDOW_HIDDEN
#undef SDL_WINDOW_HIGH_PIXEL_DENSITY
#undef SDL_WINDOW_MAXIMIZED
#undef SDL_WINDOW_RESIZABLE
#undef SDL_WINDOW_VULKAN
#undef SDL_WINDOW_METAL

export {
    using enum ::SDL_Scancode;
    using ::SDL_Scancode;
    using ::SDL_Rect;
    using ::SDL_StartTextInput;
    using ::SDL_StopTextInput;
    using ::SDL_ClearComposition;
    using ::SDL_SetTextInputArea;
    using ::SDL_DestroyCursor;
    using ::SDL_ShowCursor;
    using ::SDL_HideCursor;
    using ::SDL_EVENT_KEY_DOWN;
    using ::SDL_EVENT_KEY_UP;
    using ::SDL_EVENT_MOUSE_MOTION;
    using ::SDL_EVENT_MOUSE_BUTTON_DOWN;
    using ::SDL_EVENT_MOUSE_BUTTON_UP;
    using ::SDL_EVENT_MOUSE_WHEEL;
    using ::SDL_EVENT_TEXT_INPUT;
    using ::SDL_EVENT_TEXT_EDITING;
    using ::SDL_EVENT_WINDOW_FOCUS_LOST;
    using ::SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    using ::SDL_CreateSystemCursor;
    using ::SDL_CreateWindow;
    using ::SDL_Vulkan_CreateSurface;
    using ::SDL_Vulkan_GetInstanceExtensions;
    using ::SDL_Cursor;
    using ::SDL_DestroyWindow;
    using ::SDL_Event;
    using ::SDL_EVENT_QUIT;
    using ::SDL_EVENT_WINDOW_MAXIMIZED;
    using ::SDL_EVENT_WINDOW_MINIMIZED;
    using ::SDL_EVENT_WINDOW_RESIZED;
    using ::SDL_EVENT_WINDOW_RESTORED;
    using ::SDL_GetError;
    using ::SDL_GetWindowFlags;
    using ::SDL_GetWindowPosition;
    using ::SDL_GetWindowSize;
    using ::SDL_GetWindowSizeInPixels;
    using ::SDL_Init;
    using ::SDL_PollEvent;
    using ::SDL_AddEventWatch;
    using ::SDL_RemoveEventWatch;
    using ::SDL_PushEvent;
    using ::SDL_IsMainThread;
    using ::SDL_GetWindowID;
    using ::SDL_EVENT_WINDOW_EXPOSED;
    using ::SDL_SetCursor;
    using ::SDL_SetWindowSize;
    using ::SDL_SetWindowTitle;
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
    using ::SDL_WasInit;
    using ::SDL_Window;
#ifdef __APPLE__
    using ::SDL_Metal_CreateView;
    using ::SDL_Metal_DestroyView;
    using ::SDL_Metal_GetLayer;
#endif
    inline constexpr auto SDL_INIT_VIDEO                = hitagi::interop::sdl_init_video;
    inline constexpr auto SDL_WINDOW_HIDDEN             = hitagi::interop::sdl_window_hidden;
    inline constexpr auto SDL_WINDOW_HIGH_PIXEL_DENSITY = hitagi::interop::sdl_window_high_pixel_density;
    inline constexpr auto SDL_WINDOW_MAXIMIZED          = hitagi::interop::sdl_window_maximized;
    inline constexpr auto SDL_WINDOW_RESIZABLE          = hitagi::interop::sdl_window_resizable;
    inline constexpr auto SDL_WINDOW_VULKAN             = hitagi::interop::sdl_window_vulkan;
    inline constexpr auto SDL_WINDOW_METAL              = hitagi::interop::sdl_window_metal;
}
