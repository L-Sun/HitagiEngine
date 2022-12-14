#include "win32_application.hpp"

#include <hitagi/hid/input_manager.hpp>

#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <tracy/Tracy.hpp>

#include <windowsx.h>
#include <psapi.h>

namespace hitagi {

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
    RECT client_rect{
        .left   = CW_USEDEFAULT,
        .top    = CW_USEDEFAULT,
        .right  = CW_USEDEFAULT + static_cast<LONG>(m_Config.width),
        .bottom = CW_USEDEFAULT + static_cast<LONG>(m_Config.height),
    };
    AdjustWindowRect(&client_rect, WS_OVERLAPPEDWINDOW, false);
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
    Rect rect = m_Rect;
    GetClientRect(m_Window, reinterpret_cast<RECT*>(&m_Rect));

    auto last_width  = rect.right - rect.left;
    auto last_height = rect.bottom - rect.top;
    auto curr_width  = m_Rect.right - m_Rect.left;
    auto curr_height = m_Rect.bottom - m_Rect.top;

    if (last_width != curr_width || last_height != curr_height) {
        m_SizeChanged   = true;
        m_Config.width  = curr_width;
        m_Config.height = curr_height;
    }
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
            p_this->m_Quit = true;
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
                input_manager->UpdateKeyState(VirtualKeyCode::MOUSE_L_BUTTON, true);

            if (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK)
                input_manager->UpdateKeyState(VirtualKeyCode::MOUSE_R_BUTTON, true);

            if (message == WM_MBUTTONDOWN || message == WM_MBUTTONDBLCLK)
                input_manager->UpdateKeyState(VirtualKeyCode::MOUSE_M_BUTTON, true);

            return 0;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            if (message == WM_LBUTTONUP)
                input_manager->UpdateKeyState(VirtualKeyCode::MOUSE_L_BUTTON, false);

            if (message == WM_RBUTTONUP)
                input_manager->UpdateKeyState(VirtualKeyCode::MOUSE_R_BUTTON, false);

            if (message == WM_MBUTTONUP)
                input_manager->UpdateKeyState(VirtualKeyCode::MOUSE_M_BUTTON, false);

            return 0;
        }
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP: {
            bool down = (message == WM_KEYDOWN || message == WM_SYSKEYDOWN);
            if (w_param < static_cast<int>(VirtualKeyCode::NUM))
                input_manager->UpdateKeyState(static_cast<VirtualKeyCode>(w_param), down);

            input_manager->UpdateKeyState(VirtualKeyCode::KEY_L_CTRL, w_param & static_cast<int>(VirtualKeyCode::KEY_L_CTRL));
            input_manager->UpdateKeyState(VirtualKeyCode::KEY_R_CTRL, w_param & static_cast<int>(VirtualKeyCode::KEY_R_CTRL));
            input_manager->UpdateKeyState(VirtualKeyCode::KEY_L_SHIFT, w_param & static_cast<int>(VirtualKeyCode::KEY_L_SHIFT));
            input_manager->UpdateKeyState(VirtualKeyCode::KEY_R_SHIFT, w_param & static_cast<int>(VirtualKeyCode::KEY_R_SHIFT));
            input_manager->UpdateKeyState(VirtualKeyCode::KEY_L_ALT, w_param & static_cast<int>(VirtualKeyCode::KEY_L_ALT));
            input_manager->UpdateKeyState(VirtualKeyCode::KEY_R_ALT, w_param & static_cast<int>(VirtualKeyCode::KEY_R_ALT));
            return 0;
        }
        case WM_MOUSEMOVE:
            input_manager->UpdatePointerState(static_cast<float>(GET_X_LPARAM(l_param)), static_cast<float>(GET_Y_LPARAM(l_param)));
            return 0;
        case WM_MOUSEWHEEL:
            input_manager->UpdateWheelState(0.0f, static_cast<float>(GET_WHEEL_DELTA_WPARAM(w_param)) / static_cast<float>(WHEEL_DELTA));
            return 0;
        case WM_MOUSEHWHEEL:
            input_manager->UpdateWheelState(static_cast<float>(GET_WHEEL_DELTA_WPARAM(w_param)) / static_cast<float>(WHEEL_DELTA), 0.0f);
            return 0;
        case WM_CHAR: {
            std::size_t repeat_count = (HIWORD(l_param) & KF_REPEAT) == KF_REPEAT ? static_cast<size_t>(LOWORD(l_param)) : 1;
            input_manager->AppendInputText(std::u32string(repeat_count, static_cast<char32_t>(w_param)));
        }
            return 0;
        case WM_IME_CHAR: {
            input_manager->AppendInputText(std::u32string(1, static_cast<char32_t>(w_param)));
        }
            return 0;
        case WM_SIZE:
            if (w_param != SIZE_MINIMIZED) {
                p_this->UpdateRect();
                p_this->MapCursor();
            }
            return 0;
    }
    return DefWindowProc(h_wnd, message, w_param, l_param);
}

}  // namespace hitagi
