module;
#include <spdlog/logger.h>

#if defined(_WIN32)
#include <Windows.h>
#undef min
#undef max
#include <windowsx.h>
#include <psapi.h>
#include <timeapi.h>
#endif

#include <spdlog/sinks/stdout_color_sinks.h>
#include <tracy/Tracy.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>

export module app;
import std;
import utils;
import math;
import core;
import hid;

export namespace hitagi {
enum struct Cursor : std::uint8_t {
    None,
    Arrow,
    TextInput,
    ResizeAll,
    ResizeEW,
    ResizeNS,
    ResizeNESW,
    ResizeNWSE,
    Hand,
    Forbid,
};

struct AppConfig {
    std::pmr::string      title           = "Hitagi Engine";
    std::pmr::string      version         = "v0.2.0";
    std::uint32_t         width           = 800;
    std::uint32_t         height          = 800;
    std::filesystem::path asset_root_path = "assets";
    std::pmr::string      gfx_backend     = "Vulkan";
    std::pmr::string      log_level       = "info";
};

class Application : public RuntimeModule {
public:
    struct Rect {
        std::uint32_t left = 0, top = 0, right = 0, bottom = 0;
    };
    Application(AppConfig config);
    ~Application() override;

    static auto CreateApp(AppConfig config = {}) -> std::unique_ptr<Application>;
    static auto CreateApp(const std::filesystem::path& config_path) -> std::unique_ptr<Application>;

    void Tick() override;

    virtual void SetInputScreenPosition(const math::vec2u& position)     = 0;
    virtual void SetWindowTitle(std::string_view name)                   = 0;
    virtual void SetCursor(Cursor cursor)                                = 0;
    virtual void SetMousePosition(const math::vec2u& position)           = 0;
    virtual void ResizeWindow(std::uint32_t width, std::uint32_t height) = 0;

    virtual auto GetWindow() const -> utils::Window    = 0;
    virtual auto GetDpiRatio() const -> float          = 0;
    virtual auto GetMemoryUsage() const -> std::size_t = 0;
    virtual auto GetWindowRect() const -> Rect         = 0;
    virtual bool WindowSizeChanged() const             = 0;
    virtual bool WindowsMinimized() const              = 0;
    virtual bool IsQuit() const                        = 0;
    virtual void Quit()                                = 0;

    auto GetWindowWidth() const -> std::uint32_t;
    auto GetWindowHeight() const -> std::uint32_t;

    inline auto GetConfig() const noexcept { return m_Config; }

    inline auto& GetInputManager() const noexcept { return *m_InputManager; }

protected:
    core::Clock        m_Clock;
    AppConfig          m_Config;
    hid::InputManager* m_InputManager;
};
}  // namespace hitagi

#if defined(_WIN32)
namespace hitagi {
class Win32Application : public Application {
public:
    Win32Application(AppConfig config);

    void Tick() final;

    void SetInputScreenPosition(const math::vec2u& position) final;
    void SetWindowTitle(std::string_view name) final;
    void SetCursor(Cursor cursor) final;
    void SetMousePosition(const math::vec2u& position) final;
    void ResizeWindow(std::uint32_t width, std::uint32_t height) final;

    auto        GetWindow() const -> utils::Window final;
    inline auto GetWindowRect() const -> Rect final { return m_Rect; }
    inline bool WindowSizeChanged() const final { return m_SizeChanged; }
    inline bool WindowsMinimized() const final { return m_Minimized; };
    inline bool IsQuit() const final { return m_Quit; }
    inline void Quit() final { m_Quit = true; }

    auto GetDpiRatio() const -> float final;
    auto GetMemoryUsage() const -> std::size_t final;

private:
    void InitializeWindows();

    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);

    void UpdateRect();
    void MapCursor();

    bool m_LockCursor  = false;
    bool m_SizeChanged = false;
    bool m_Minimized   = false;
    bool m_Quit        = false;

    Rect m_Rect;

    mutable HWND m_Window{};
};


Win32Application::Win32Application(AppConfig config) : Application(std::move(config)) {
    InitializeWindows();
}

void Win32Application::Tick() {
    m_SizeChanged = false;

    MSG msg;
    // we use PeekMessage instead of GetMessage here
    // because we should not block the thread at anywhere
    // except the engine execution driver module
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        // translate keystroke messages into the right format
        TranslateMessage(&msg);

        // send the message to the WindowProc function
        DispatchMessage(&msg);
    }
    Application::Tick();
}

void Win32Application::InitializeWindows() {
    std::wstring title{m_Config.title.begin(), m_Config.title.end()};

    // Set time period on windows
    timeBeginPeriod(1);

    SetProcessDPIAware();

    RECT window_rect{
        .left   = CW_USEDEFAULT,
        .top    = CW_USEDEFAULT,
        .right  = CW_USEDEFAULT + static_cast<LONG>(m_Config.width),
        .bottom = CW_USEDEFAULT + static_cast<LONG>(m_Config.height),
    };
    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, false);

    // get the HINSTANCE of the Console Program
    HINSTANCE h_instance = GetModuleHandle(nullptr);

    // this struct holds information for the window class
    WNDCLASSEX wc;

    // clear out the window class for use
    ZeroMemory(&wc, sizeof(WNDCLASSEX));

    // fill in the struct with the needed information
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = h_instance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = title.c_str();

    // register the window class
    RegisterClassEx(&wc);
    m_Window = CreateWindowEx(
        0,
        title.c_str(),
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        nullptr,
        nullptr,
        h_instance,
        this);

    if (m_Window == nullptr) {
        m_Logger->error("Create window failed.");
        return;
    }
    ShowWindow(m_Window, SW_SHOW);

    UpdateRect();
    MapCursor();
}

void Win32Application::SetInputScreenPosition(const math::vec2u& position) {
    if (HIMC himc = ::ImmGetContext(m_Window)) {
        COMPOSITIONFORM composition_form = {};
        composition_form.ptCurrentPos.x  = position.x;
        composition_form.ptCurrentPos.y  = position.y;
        composition_form.dwStyle         = CFS_FORCE_POSITION;
        ::ImmSetCompositionWindow(himc, &composition_form);
        CANDIDATEFORM candidate_form  = {};
        candidate_form.dwStyle        = CFS_CANDIDATEPOS;
        candidate_form.ptCurrentPos.x = position.x;
        candidate_form.ptCurrentPos.y = position.y;
        ::ImmSetCandidateWindow(himc, &candidate_form);
        ::ImmReleaseContext(m_Window, himc);
    }
}

void Win32Application::SetWindowTitle(std::string_view title) {
    std::pmr::wstring text{title.begin(), title.end()};
    SetWindowTextW(m_Window, text.data());
}

void Win32Application::SetCursor(Cursor cursor) {
    LPTSTR win32_cursor = IDC_ARROW;
    switch (cursor) {
        case Cursor::None:
            win32_cursor = nullptr;
        case Cursor::Arrow:
            win32_cursor = IDC_ARROW;
            break;
        case Cursor::TextInput:
            win32_cursor = IDC_IBEAM;
            break;
        case Cursor::ResizeAll:
            win32_cursor = IDC_SIZEALL;
            break;
        case Cursor::ResizeEW:
            win32_cursor = IDC_SIZEWE;
            break;
        case Cursor::ResizeNS:
            win32_cursor = IDC_SIZENS;
            break;
        case Cursor::ResizeNESW:
            win32_cursor = IDC_SIZENESW;
            break;
        case Cursor::ResizeNWSE:
            win32_cursor = IDC_SIZENWSE;
            break;
        case Cursor::Hand:
            win32_cursor = IDC_HAND;
            break;
        case Cursor::Forbid:
            win32_cursor = IDC_NO;
            break;
    }
    if (win32_cursor == nullptr) {
        ::SetCursor(nullptr);
    } else {
        ::SetCursor(::LoadCursor(nullptr, win32_cursor));
    }
}

void Win32Application::SetMousePosition(const math::vec2u& position) {
    ::SetCursorPos(position.x, position.y);
}

void Win32Application::ResizeWindow(std::uint32_t width, std::uint32_t height) {
    RECT client_rect{
        .left   = static_cast<LONG>(m_Rect.left),
        .top    = static_cast<LONG>(m_Rect.top),
        .right  = static_cast<LONG>(m_Rect.left + width),
        .bottom = static_cast<LONG>(m_Rect.top + height),
    };

    // Get window position using WIN32 API
    RECT window_rect;
    ::GetWindowRect(m_Window, &window_rect);

    AdjustWindowRect(&client_rect, WS_OVERLAPPEDWINDOW, false);
    SetWindowPos(m_Window, nullptr, window_rect.left, window_rect.top,
                 client_rect.right - client_rect.left, client_rect.bottom - client_rect.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    UpdateRect();
}

auto Win32Application::GetWindow() const -> utils::Window {
    return {
        .type = utils::Window::Type::Win32,
        .ptr  = m_Window,
    };
}

float Win32Application::GetDpiRatio() const {
    unsigned dpi = GetDpiForWindow(m_Window);
    return dpi / 96.0f;
}

std::size_t Win32Application::GetMemoryUsage() const {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    return pmc.WorkingSetSize;
}

void Win32Application::UpdateRect() {
    m_SizeChanged = true;
    GetClientRect(m_Window, reinterpret_cast<RECT*>(&m_Rect));
}

void Win32Application::MapCursor() {
    if (m_LockCursor) {
        POINT ul;
        ul.x = m_Rect.left;
        ul.y = m_Rect.top;

        POINT lr;
        lr.x = m_Rect.right;
        lr.y = m_Rect.bottom;

        MapWindowPoints(m_Window, nullptr, &ul, 1);
        MapWindowPoints(m_Window, nullptr, &lr, 1);

        m_Rect.left = ul.x + 1;
        m_Rect.top  = ul.y + 1;

        m_Rect.right  = lr.x - 1;
        m_Rect.bottom = lr.y - 1;
        ClipCursor(reinterpret_cast<RECT*>(&m_Rect));
    }
}

LRESULT CALLBACK Win32Application::WindowProc(HWND h_wnd, UINT message, WPARAM w_param, LPARAM l_param) {
    ZoneScoped;

    Win32Application* p_this = nullptr;
    if (message == WM_NCCREATE) {
        p_this = static_cast<Win32Application*>(reinterpret_cast<CREATESTRUCT*>(l_param)->lpCreateParams);

        SetLastError(0);
        if (!SetWindowLongPtr(h_wnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(p_this))) {
            if (GetLastError() != 0) return false;
        }
    } else {
        p_this = reinterpret_cast<Win32Application*>(GetWindowLongPtr(h_wnd, GWLP_USERDATA));
    }
    switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            p_this->m_Quit        = true;
            p_this->m_SizeChanged = true;
            ClipCursor(nullptr);
            return 0;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK: {
            if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK)
                p_this->m_InputManager->UpdateKeyState(hid::VirtualKeyCode::MOUSE_L_BUTTON, true);

            if (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK)
                p_this->m_InputManager->UpdateKeyState(hid::VirtualKeyCode::MOUSE_R_BUTTON, true);

            if (message == WM_MBUTTONDOWN || message == WM_MBUTTONDBLCLK)
                p_this->m_InputManager->UpdateKeyState(hid::VirtualKeyCode::MOUSE_M_BUTTON, true);

            return 0;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            if (message == WM_LBUTTONUP)
                p_this->m_InputManager->UpdateKeyState(hid::VirtualKeyCode::MOUSE_L_BUTTON, false);

            if (message == WM_RBUTTONUP)
                p_this->m_InputManager->UpdateKeyState(hid::VirtualKeyCode::MOUSE_R_BUTTON, false);

            if (message == WM_MBUTTONUP)
                p_this->m_InputManager->UpdateKeyState(hid::VirtualKeyCode::MOUSE_M_BUTTON, false);

            return 0;
        }
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP: {
            bool down = (message == WM_KEYDOWN || message == WM_SYSKEYDOWN);
            if (w_param < static_cast<int>(hid::VirtualKeyCode::NUM))
                p_this->m_InputManager->UpdateKeyState(static_cast<hid::VirtualKeyCode>(w_param), down);

            p_this->m_InputManager->UpdateKeyState(hid::VirtualKeyCode::KEY_L_CTRL, w_param & static_cast<int>(hid::VirtualKeyCode::KEY_L_CTRL));
            p_this->m_InputManager->UpdateKeyState(hid::VirtualKeyCode::KEY_R_CTRL, w_param & static_cast<int>(hid::VirtualKeyCode::KEY_R_CTRL));
            p_this->m_InputManager->UpdateKeyState(hid::VirtualKeyCode::KEY_L_SHIFT, w_param & static_cast<int>(hid::VirtualKeyCode::KEY_L_SHIFT));
            p_this->m_InputManager->UpdateKeyState(hid::VirtualKeyCode::KEY_R_SHIFT, w_param & static_cast<int>(hid::VirtualKeyCode::KEY_R_SHIFT));
            p_this->m_InputManager->UpdateKeyState(hid::VirtualKeyCode::KEY_L_ALT, w_param & static_cast<int>(hid::VirtualKeyCode::KEY_L_ALT));
            p_this->m_InputManager->UpdateKeyState(hid::VirtualKeyCode::KEY_R_ALT, w_param & static_cast<int>(hid::VirtualKeyCode::KEY_R_ALT));
            return 0;
        }
        case WM_MOUSEMOVE:
            p_this->m_InputManager->UpdatePointerState(static_cast<float>(GET_X_LPARAM(l_param)), static_cast<float>(GET_Y_LPARAM(l_param)));
            return 0;
        case WM_MOUSEWHEEL:
            p_this->m_InputManager->UpdateWheelState(0.0f, static_cast<float>(GET_WHEEL_DELTA_WPARAM(w_param)) / static_cast<float>(WHEEL_DELTA));
            return 0;
        case WM_MOUSEHWHEEL:
            p_this->m_InputManager->UpdateWheelState(static_cast<float>(GET_WHEEL_DELTA_WPARAM(w_param)) / static_cast<float>(WHEEL_DELTA), 0.0f);
            return 0;
        case WM_CHAR: {
            std::size_t repeat_count = (HIWORD(l_param) & KF_REPEAT) == KF_REPEAT ? static_cast<size_t>(LOWORD(l_param)) : 1;
            p_this->m_InputManager->AppendInputText(std::u32string(repeat_count, static_cast<char32_t>(w_param)));
        }
            return 0;
        case WM_IME_CHAR: {
            p_this->m_InputManager->AppendInputText(std::u32string(1, static_cast<char32_t>(w_param)));
        }
            return 0;
        case WM_SIZE:
            p_this->m_Minimized = w_param == SIZE_MINIMIZED;
            p_this->UpdateRect();
            p_this->MapCursor();
            return 0;
    }
    return DefWindowProc(h_wnd, message, w_param, l_param);
}

}  // namespace hitagi
#endif

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
