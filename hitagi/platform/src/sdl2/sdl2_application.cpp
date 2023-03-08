#include "sdl2_application.hpp"

#include <hitagi/utils/exceptions.hpp>

#include <spdlog/logger.h>

namespace hitagi {
SDL2Application::SDL2Application(AppConfig config) : Application(std::move(config)) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        const auto error_message = fmt::format("SDL_Init failed: {}", SDL_GetError());
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    m_Window = SDL_CreateWindow(
        m_Config.title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_Config.width,
        m_Config.height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_VULKAN);
    if (!m_Window) {
        const auto error_message = fmt::format("SDL_CreateWindow failed: {}", SDL_GetError());
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }
}

SDL2Application::~SDL2Application() {
    SDL_DestroyWindow(m_Window);
}

void SDL2Application::Tick() {
    m_SizeChanged = false;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                m_Quit = true;
                break;
            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                        m_SizeChanged   = true;
                        m_Config.width  = event.window.data1;
                        m_Config.height = event.window.data2;
                        break;
                }
                break;
        }
    }

    Application::Tick();
}

void SDL2Application::SetInputScreenPosition(const math::vec2u& position) {
    throw utils::NoImplemented();
}

void SDL2Application::SetWindowTitle(std::string_view name) {
    SDL_SetWindowTitle(m_Window, name.data());
}

void SDL2Application::SetCursor(Cursor cursor) {
    SDL_Cursor* cursor_ptr = nullptr;
    switch (cursor) {
        case Cursor::None:
            cursor_ptr = nullptr;
            break;
        case Cursor::Arrow:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
            break;
        case Cursor::TextInput:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
            break;
        case Cursor::Hand:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
            break;
        case Cursor::ResizeNWSE:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
            break;
        case Cursor::ResizeNESW:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
            break;
        case Cursor::ResizeEW:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
            break;
        case Cursor::ResizeNS:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
            break;
        case Cursor::ResizeAll:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
            break;
        case Cursor::Forbid:
            cursor_ptr = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);
            break;
    }
    SDL_SetCursor(cursor_ptr);
}

void SDL2Application::SetMousePosition(const math::vec2u& position) {
    SDL_WarpMouseInWindow(m_Window, position.x, position.y);
}

void SDL2Application::ResizeWindow(std::uint32_t width, std::uint32_t height) {
    SDL_SetWindowSize(m_Window, width, height);
}

auto SDL2Application::GetWindow() const -> utils::Window {
    return {
        .type = utils::Window::Type::SDL2,
        .ptr  = m_Window,
    };
}

auto SDL2Application::GetDpiRatio() const -> float {
    return 1.0f;
}

auto SDL2Application::GetMemoryUsage() const -> std::size_t {
    return 0;
}

auto SDL2Application::GetWindowsRect() const -> Rect {
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