module;
#if defined(_WIN32)
#include "interop/win32_macros.hpp"
#endif
#include "interop/tracy_macros.hpp"

module platform:sdl;
import interop.sdl;
import interop.spdlog;
import interop.tracy;

import std;
import utils;
import math;
import core;
import hid;

import :host;

namespace hitagi::platform {

class SDLPlatformHost final : public PlatformHost {
public:
    SDLPlatformHost(PlatformConfig config);
    ~SDLPlatformHost() final;

    void PollEvents() final;

    void SetTextInputSession(std::uint64_t session) final;
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
    bool        WindowMaximized() const final;
    inline void Quit() final { m_Quit = true; }
    inline bool IsQuit() const final { return m_Quit; }

private:
    static bool WatchLiveResize(void* userdata, SDL_Event* event) noexcept;

    SDL_Window* m_Window = nullptr;

    std::map<Cursor, SDL_Cursor*> m_Cursors;
    bool                          m_Composing = false;

    bool m_Quit        = false;
    bool m_SizeChanged = false;
    bool m_Minimized   = false;
};

SDLPlatformHost::SDLPlatformHost(PlatformConfig config) : PlatformHost(std::move(config)) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        const auto error_message = std::format("SDL_Init failed: {}", SDL_GetError());
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    auto window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    window_flags |= m_Config.surface == Surface::Metal ? SDL_WINDOW_METAL : SDL_WINDOW_VULKAN;
    if (m_Config.headless) {
        window_flags |= SDL_WINDOW_HIDDEN;
    }
    if (m_Config.maximized) {
        window_flags |= SDL_WINDOW_MAXIMIZED;
    }

    m_Window = SDL_CreateWindow(
        m_Config.title.c_str(),
        m_Config.width,
        m_Config.height,
        window_flags);
    if (!m_Window) {
        const auto error_message = std::format("SDL_CreateWindow failed: {}", SDL_GetError());
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    if (!SDL_AddEventWatch(WatchLiveResize, this)) {
        const auto error_message = std::format("SDL_AddEventWatch failed: {}", SDL_GetError());
        SDL_DestroyWindow(m_Window);
        throw std::runtime_error(error_message);
    }
}

bool SDLPlatformHost::WatchLiveResize(void* userdata, SDL_Event* event) noexcept {
    // Cocoa's live-resize loop blocks SDL_PollEvent. SDL sends these exposes
    // synchronously so we can advance a frame without pumping events again.
    if (event->type != SDL_EVENT_WINDOW_EXPOSED || event->window.data1 != 1 || !SDL_IsMainThread()) return true;
    auto& app = *static_cast<SDLPlatformHost*>(userdata);
    if (event->window.windowID != SDL_GetWindowID(app.m_Window)) return true;
    ZoneScopedN("SDL Modal Frame");
    app.TickModalFrame();
    return true;
}

SDLPlatformHost::~SDLPlatformHost() {
    SDL_RemoveEventWatch(WatchLiveResize, this);
    for (const auto& [_, cursor] : m_Cursors) SDL_DestroyCursor(cursor);
    SDL_DestroyWindow(m_Window);
}

auto SDLKey(SDL_Scancode code) -> hid::VirtualKeyCode {
    using enum hid::VirtualKeyCode;
    switch (code) {
        case SDL_SCANCODE_BACKSPACE:
            return KEY_BACKSPACE;
        case SDL_SCANCODE_TAB:
            return KEY_TAB;
        case SDL_SCANCODE_RETURN:
            return KEY_ENTER;
        case SDL_SCANCODE_ESCAPE:
            return KEY_ESCAPE;
        case SDL_SCANCODE_SPACE:
            return KEY_SPACE;
        case SDL_SCANCODE_PAGEUP:
            return KEY_PAGE_UP;
        case SDL_SCANCODE_PAGEDOWN:
            return KEY_PAGE_DOWN;
        case SDL_SCANCODE_END:
            return KEY_END;
        case SDL_SCANCODE_HOME:
            return KEY_HOME;
        case SDL_SCANCODE_LEFT:
            return KEY_LEFT;
        case SDL_SCANCODE_UP:
            return KEY_UP;
        case SDL_SCANCODE_RIGHT:
            return KEY_RIGHT;
        case SDL_SCANCODE_DOWN:
            return KEY_DOWN;
        case SDL_SCANCODE_INSERT:
            return KEY_INSERT;
        case SDL_SCANCODE_DELETE:
            return KEY_DELETE;
        case SDL_SCANCODE_LSHIFT:
            return KEY_L_SHIFT;
        case SDL_SCANCODE_RSHIFT:
            return KEY_R_SHIFT;
        case SDL_SCANCODE_LCTRL:
            return KEY_L_CTRL;
        case SDL_SCANCODE_RCTRL:
            return KEY_R_CTRL;
        case SDL_SCANCODE_LALT:
            return KEY_L_ALT;
        case SDL_SCANCODE_RALT:
            return KEY_R_ALT;
        case SDL_SCANCODE_LGUI:
            return KEY_L_CTRL;
        case SDL_SCANCODE_RGUI:
            return KEY_R_CTRL;
        case SDL_SCANCODE_CAPSLOCK:
            return KEY_CAPS_LOCK;
        case SDL_SCANCODE_NUMLOCKCLEAR:
            return KEY_NUM_LOCK;
        case SDL_SCANCODE_SCROLLLOCK:
            return KEY_SCROLL_LOCK;
        case SDL_SCANCODE_SEMICOLON:
            return KEY_SEMICOLON;
        case SDL_SCANCODE_EQUALS:
            return KEY_EQUAL;
        case SDL_SCANCODE_COMMA:
            return KEY_COMMA;
        case SDL_SCANCODE_MINUS:
            return KEY_MINUS;
        case SDL_SCANCODE_PERIOD:
            return KEY_PERIOD;
        case SDL_SCANCODE_SLASH:
            return KEY_SLASH;
        case SDL_SCANCODE_GRAVE:
            return KEY_GRAVEACCENT;
        case SDL_SCANCODE_LEFTBRACKET:
            return KEY_LEFTBRACKET;
        case SDL_SCANCODE_BACKSLASH:
            return KEY_BACKSLASH;
        case SDL_SCANCODE_RIGHTBRACKET:
            return KEY_RIGHTBRACKET;
        case SDL_SCANCODE_APOSTROPHE:
            return KEY_APOSTROPHE;
        case SDL_SCANCODE_A:
            return KEY_A;
        case SDL_SCANCODE_B:
            return KEY_B;
        case SDL_SCANCODE_C:
            return KEY_C;
        case SDL_SCANCODE_D:
            return KEY_D;
        case SDL_SCANCODE_E:
            return KEY_E;
        case SDL_SCANCODE_F:
            return KEY_F;
        case SDL_SCANCODE_G:
            return KEY_G;
        case SDL_SCANCODE_H:
            return KEY_H;
        case SDL_SCANCODE_I:
            return KEY_I;
        case SDL_SCANCODE_J:
            return KEY_J;
        case SDL_SCANCODE_K:
            return KEY_K;
        case SDL_SCANCODE_L:
            return KEY_L;
        case SDL_SCANCODE_M:
            return KEY_M;
        case SDL_SCANCODE_N:
            return KEY_N;
        case SDL_SCANCODE_O:
            return KEY_O;
        case SDL_SCANCODE_P:
            return KEY_P;
        case SDL_SCANCODE_Q:
            return KEY_Q;
        case SDL_SCANCODE_R:
            return KEY_R;
        case SDL_SCANCODE_S:
            return KEY_S;
        case SDL_SCANCODE_T:
            return KEY_T;
        case SDL_SCANCODE_U:
            return KEY_U;
        case SDL_SCANCODE_V:
            return KEY_V;
        case SDL_SCANCODE_W:
            return KEY_W;
        case SDL_SCANCODE_X:
            return KEY_X;
        case SDL_SCANCODE_Y:
            return KEY_Y;
        case SDL_SCANCODE_Z:
            return KEY_Z;
        case SDL_SCANCODE_0:
            return KEY_0;
        case SDL_SCANCODE_1:
            return KEY_1;
        case SDL_SCANCODE_2:
            return KEY_2;
        case SDL_SCANCODE_3:
            return KEY_3;
        case SDL_SCANCODE_4:
            return KEY_4;
        case SDL_SCANCODE_5:
            return KEY_5;
        case SDL_SCANCODE_6:
            return KEY_6;
        case SDL_SCANCODE_7:
            return KEY_7;
        case SDL_SCANCODE_8:
            return KEY_8;
        case SDL_SCANCODE_9:
            return KEY_9;
        case SDL_SCANCODE_F1:
            return KEY_F1;
        case SDL_SCANCODE_F2:
            return KEY_F2;
        case SDL_SCANCODE_F3:
            return KEY_F3;
        case SDL_SCANCODE_F4:
            return KEY_F4;
        case SDL_SCANCODE_F5:
            return KEY_F5;
        case SDL_SCANCODE_F6:
            return KEY_F6;
        case SDL_SCANCODE_F7:
            return KEY_F7;
        case SDL_SCANCODE_F8:
            return KEY_F8;
        case SDL_SCANCODE_F9:
            return KEY_F9;
        case SDL_SCANCODE_F10:
            return KEY_F10;
        case SDL_SCANCODE_F11:
            return KEY_F11;
        case SDL_SCANCODE_F12:
            return KEY_F12;
        case SDL_SCANCODE_F13:
            return KEY_F13;
        case SDL_SCANCODE_F14:
            return KEY_F14;
        case SDL_SCANCODE_F15:
            return KEY_F15;
        case SDL_SCANCODE_F16:
            return KEY_F16;
        case SDL_SCANCODE_F17:
            return KEY_F17;
        case SDL_SCANCODE_F18:
            return KEY_F18;
        case SDL_SCANCODE_F19:
            return KEY_F19;
        case SDL_SCANCODE_F20:
            return KEY_F20;
        case SDL_SCANCODE_F21:
            return KEY_F21;
        case SDL_SCANCODE_F22:
            return KEY_F22;
        case SDL_SCANCODE_F23:
            return KEY_F23;
        case SDL_SCANCODE_F24:
            return KEY_F24;
        default:
            return NONE;
    }
}

// SDL delivers UTF-8; GUI composition offsets use UTF-16 code units.
auto SDLText(std::string_view bytes) -> std::u32string {
    std::u32string result;
    for (std::size_t i = 0; i < bytes.size();) {
        auto     c       = static_cast<unsigned char>(bytes[i++]);
        char32_t code    = c;
        unsigned count   = 0;
        char32_t minimum = 0;
        if (c >= 0xc2 && c <= 0xdf) {
            code    = c & 0x1f;
            count   = 1;
            minimum = 0x80;
        } else if (c >= 0xe0 && c <= 0xef) {
            code    = c & 0x0f;
            count   = 2;
            minimum = 0x800;
        } else if (c >= 0xf0 && c <= 0xf4) {
            code    = c & 7;
            count   = 3;
            minimum = 0x10000;
        } else if (c >= 0x80) {
            result.push_back(0xfffd);
            continue;
        }
        bool valid = i + count <= bytes.size();
        for (unsigned n = 0; valid && n < count; ++n) {
            auto next = static_cast<unsigned char>(bytes[i]);
            if ((next & 0xc0) != 0x80)
                valid = false;
            else {
                code = (code << 6) | (next & 0x3f);
                ++i;
            }
        }
        if (!valid || code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) code = 0xfffd;
        result.push_back(code);
    }
    return result;
}

auto SDLText16(std::u32string_view text) -> std::u16string {
    std::u16string result;
    for (auto code : text) {
        if (code <= 0xffff)
            result.push_back(static_cast<char16_t>(code));
        else {
            code -= 0x10000;
            result.push_back(0xd800 + (code >> 10));
            result.push_back(0xdc00 + (code & 0x3ff));
        }
    }
    return result;
}

void SDLPlatformHost::PollEvents() {
    m_SizeChanged = false;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_MOUSE_MOTION:
                m_Input->UpdatePointerState(event.motion.x * GetDpiRatio(), event.motion.y * GetDpiRatio());
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                m_Input->UpdateWheelState(event.wheel.y, event.wheel.x);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                const auto key = event.button.button == 1 ? hid::VirtualKeyCode::MOUSE_L_BUTTON : event.button.button == 2 ? hid::VirtualKeyCode::MOUSE_M_BUTTON
                                                                                              : event.button.button == 3   ? hid::VirtualKeyCode::MOUSE_R_BUTTON
                                                                                                                           : hid::VirtualKeyCode::NONE;
                if (key != hid::VirtualKeyCode::NONE) m_Input->UpdateKeyState(key, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                break;
            }
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                const auto key  = SDLKey(event.key.scancode);
                const bool down = event.type == SDL_EVENT_KEY_DOWN;
                if (key != hid::VirtualKeyCode::NONE) m_Input->UpdateKeyState(key, down);
                using enum hid::VirtualKeyCode;
                const bool shift   = m_Input->GetBool(KEY_L_SHIFT) || m_Input->GetBool(KEY_R_SHIFT);
                const bool control = m_Input->GetBool(KEY_L_CTRL) || m_Input->GetBool(KEY_R_CTRL);
                m_Input->UpdateKeyState(KEY_SHIFT, shift);
                m_Input->UpdateKeyState(KEY_CTRL, control);
                m_Input->UpdateKeyState(KEY_ALT, m_Input->GetBool(KEY_L_ALT) || m_Input->GetBool(KEY_R_ALT));
                const auto value = static_cast<std::uint32_t>(key);
                if (down && !m_Composing && (value == 8 || value == 9 || value == 13 || value == 27 || (value >= 35 && value <= 40) || value == 46 || value == 65 || value == 89 || value == 90))
                    m_Input->PushTextInput({.kind = hid::TextInputEvent::Kind::Key, .key = value, .shift = shift, .control = control});
                break;
            }
            case SDL_EVENT_TEXT_EDITING: {
                if (!m_Composing) m_Input->PushTextInput({.kind = hid::TextInputEvent::Kind::Start});
                m_Composing      = true;
                const auto text  = SDLText(event.edit.text);
                const auto caret = SDLText16(std::u32string_view(text).substr(0, std::min(text.size(), static_cast<std::size_t>(std::max(0, event.edit.start))))).size();
                m_Input->PushTextInput({.kind = hid::TextInputEvent::Kind::Update, .text = SDLText16(text), .caret = static_cast<std::uint32_t>(caret)});
                break;
            }
            case SDL_EVENT_TEXT_INPUT: {
                const auto text = SDLText(event.text.text);
                m_Input->PushTextInput({.kind = hid::TextInputEvent::Kind::Commit, .text = SDLText16(text)});
                m_Input->AppendInputText(text);
                if (m_Composing) m_Input->PushTextInput({.kind = hid::TextInputEvent::Kind::End});
                m_Composing = false;
                break;
            }
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                m_Input->PushTextInput({.kind = hid::TextInputEvent::Kind::Blur});
                SetTextInputSession(0);
                m_Input->ResetInputState();
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            case SDL_EVENT_QUIT:
                m_Quit        = true;
                m_SizeChanged = true;
                break;
            case SDL_EVENT_WINDOW_MINIMIZED:
                m_Minimized = true;
                break;
            case SDL_EVENT_WINDOW_MAXIMIZED:
                m_Minimized        = false;
                m_Config.maximized = true;
                break;
            case SDL_EVENT_WINDOW_RESTORED:
                m_Minimized        = false;
                m_Config.maximized = false;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                m_SizeChanged = true;
                if (!m_Minimized && !WindowMaximized()) {
                    m_Config.width  = event.window.data1;
                    m_Config.height = event.window.data2;
                }
                break;
        }
    }

    PlatformHost::PollEvents();
}

void SDLPlatformHost::SetTextInputSession(std::uint64_t session) {
    if (session == m_Input->GetTextInputSession()) return;
    SDL_ClearComposition(m_Window);
    SDL_StopTextInput(m_Window);
    m_Composing = false;
    PlatformHost::SetTextInputSession(session);
    if (session) SDL_StartTextInput(m_Window);
}

void SDLPlatformHost::SetInputScreenPosition(const math::vec2u& position) {
    const auto dpi = GetDpiRatio();
    SDL_Rect   area{static_cast<int>(position.x / dpi), static_cast<int>(position.y / dpi), 1, 1};
    SDL_SetTextInputArea(m_Window, &area, 0);
}

void SDLPlatformHost::SetWindowTitle(std::string_view name) {
    SDL_SetWindowTitle(m_Window, name.data());
}

void SDLPlatformHost::SetCursor(Cursor cursor) {
    if (cursor == Cursor::None) {
        SDL_HideCursor();
        return;
    }
    SDL_ShowCursor();
    if (auto found = m_Cursors.find(cursor); found != m_Cursors.end()) {
        SDL_SetCursor(found->second);
        return;
    }
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
    if (cursor_ptr) {
        m_Cursors.emplace(cursor, cursor_ptr);
        SDL_SetCursor(cursor_ptr);
    }
}

void SDLPlatformHost::SetMousePosition(const math::vec2u& position) {
    SDL_WarpMouseInWindow(m_Window, position.x / GetDpiRatio(), position.y / GetDpiRatio());
}

void SDLPlatformHost::ResizeWindow(std::uint32_t width, std::uint32_t height) {
    SDL_SetWindowSize(m_Window, static_cast<int>(width / GetDpiRatio()), static_cast<int>(height / GetDpiRatio()));
}

auto SDLPlatformHost::GetWindow() const -> utils::Window {
    return {
        .type = utils::Window::Type::SDL3,
        .ptr  = m_Window,
    };
}

auto SDLPlatformHost::GetDpiRatio() const -> float {
    int width = 0, height = 0, pixels_width = 0, pixels_height = 0;
    SDL_GetWindowSize(m_Window, &width, &height);
    SDL_GetWindowSizeInPixels(m_Window, &pixels_width, &pixels_height);
    return width > 0 ? static_cast<float>(pixels_width) / width : 1.0f;
}

auto SDLPlatformHost::GetMemoryUsage() const -> std::size_t {
    return 0;
}

bool SDLPlatformHost::WindowMaximized() const {
    return (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_MAXIMIZED) != 0;
}

auto SDLPlatformHost::GetWindowRect() const -> Rect {
    int x, y;
    SDL_GetWindowPosition(m_Window, &x, &y);
    int w, h;
    SDL_GetWindowSizeInPixels(m_Window, &w, &h);
    x = static_cast<int>(x * GetDpiRatio());
    y = static_cast<int>(y * GetDpiRatio());
    return {
        .left   = static_cast<std::uint32_t>(x),
        .top    = static_cast<std::uint32_t>(y),
        .right  = static_cast<std::uint32_t>(x + w),
        .bottom = static_cast<std::uint32_t>(y + h),
    };
}

}  // namespace hitagi::platform
