module;
#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>

#include <spdlog/logger.h>

module app:sdl3;
import std;
import :base;

namespace hitagi {

class SDL3Application final : public Application {
public:
    SDL3Application(AppConfig config);
    ~SDL3Application() final;

    void Tick() final;

    void SetInputScreenPosition(const math::vec2u& position) final;
    void SetWindowTitle(std::string_view name) final;
    void SetCursor(Cursor cursor) final;
    void SetMousePosition(const math::vec2u& position) final;
    void ResizeWindow(std::uint32_t width, std::uint32_t height) final;

    auto GetWindow() const -> utils::Window final;
    auto GetDpiRatio() const -> float final;
    auto GetMemoryUsage() const -> std::size_t final;
    auto GetWindowRect() const -> Rect final;

    inline bool WindowSizeChanged() const final { return m_SizeChanged; }
    inline bool WindowsMinimized() const final { return m_Minimized; };
    inline void Quit() final { m_Quit = true; }
    inline bool IsQuit() const final { return m_Quit; }

private:
    SDL_Window* m_Window = nullptr;

    bool m_Quit        = false;
    bool m_SizeChanged = false;
    bool m_Minimized   = false;
};

}  // namespace hitagi

namespace hitagi {
SDL3Application::SDL3Application(AppConfig config) : Application(std::move(config)) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        const auto error_message = std::format("SDL_Init failed: {}", SDL_GetError());
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    m_Window = SDL_CreateWindow(
        m_Config.title.c_str(),
        m_Config.width,
        m_Config.height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN);
    if (!m_Window) {
        const auto error_message = std::format("SDL_CreateWindow failed: {}", SDL_GetError());
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }
}

SDL3Application::~SDL3Application() {
    SDL_DestroyWindow(m_Window);
}

void SDL3Application::Tick() {
    m_SizeChanged = false;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                m_Quit        = true;
                m_SizeChanged = true;
                break;
            case SDL_EVENT_WINDOW_MINIMIZED:
                m_Minimized = true;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                m_SizeChanged   = true;
                m_Config.width  = event.window.data1;
                m_Config.height = event.window.data2;
                break;
        }
    }

    Application::Tick();
}

void SDL3Application::SetInputScreenPosition(const math::vec2u& position) {
    throw utils::NoImplemented();
}

void SDL3Application::SetWindowTitle(std::string_view name) {
    SDL_SetWindowTitle(m_Window, name.data());
}

void SDL3Application::SetCursor(Cursor cursor) {
    SDL_Cursor* cursor_ptr = nullptr;
    switch (cursor) {
        case Cursor::None:
            cursor_ptr = nullptr;
            break;
        case Cursor::Arrow:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
            break;
        case Cursor::TextInput:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
            break;
        case Cursor::Hand:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
            break;
        case Cursor::ResizeNWSE:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE);
            break;
        case Cursor::ResizeNESW:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE);
            break;
        case Cursor::ResizeEW:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
            break;
        case Cursor::ResizeNS:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);
            break;
        case Cursor::ResizeAll:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
            break;
        case Cursor::Forbid:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED);
            break;
    }
    SDL_SetCursor(cursor_ptr);
}

void SDL3Application::SetMousePosition(const math::vec2u& position) {
    SDL_WarpMouseInWindow(m_Window, position.x, position.y);
}

void SDL3Application::ResizeWindow(std::uint32_t width, std::uint32_t height) {
    SDL_SetWindowSize(m_Window, width, height);
}

auto SDL3Application::GetWindow() const -> utils::Window {
    return {
        .type = utils::Window::Type::SDL3,
        .ptr  = m_Window,
    };
}

auto SDL3Application::GetDpiRatio() const -> float {
    return 1.0f;
}

auto SDL3Application::GetMemoryUsage() const -> std::size_t {
    return 0;
}

auto SDL3Application::GetWindowRect() const -> Rect {
    int x, y;
    SDL_GetWindowPosition(m_Window, &x, &y);
    int w, h;
    SDL_GetWindowSize(m_Window, &w, &h);
    return {
        static_cast<std::uint32_t>(x),
        static_cast<std::uint32_t>(y),
        static_cast<std::uint32_t>(x + w),
        static_cast<std::uint32_t>(y + h),
    };
}

}  // namespace hitagi