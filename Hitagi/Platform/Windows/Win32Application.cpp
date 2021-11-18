#include "Win32Application.hpp"

#include "InputManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <windowsx.h>

namespace Hitagi {

extern GfxConfiguration      g_Config;
std::unique_ptr<Application> g_App = std::make_unique<Win32Application>(g_Config);

int Win32Application::Initialize() {
    m_Logger->info("Initialize");

    // Set time period on windows
    timeBeginPeriod(1);

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
    wc.lpszClassName = L"HitagiEngine";

    // register the window class
    RegisterClassEx(&wc);
    const std::wstring title(m_Config.app_name.begin(), m_Config.app_name.end());
    m_Window = CreateWindowEx(
        0,
        L"HitagiEngine",
        title.c_str(),                 // title
        WS_OVERLAPPEDWINDOW,           // Window style
        CW_USEDEFAULT, CW_USEDEFAULT,  // Position (x, y)
        m_Config.screen_width,         // Width
        m_Config.screen_height,        // Height
        nullptr,                       // Parent window
        nullptr,                       // Menu
        h_instance,                    // Instance handle
        this                           //  Pass pointer to current object
    );
    if (m_Window == nullptr) {
        m_Logger->error("Create window failed.");
        return 1;
    }
    ShowWindow(m_Window, SW_SHOW);

    GetClientRect(m_Window, reinterpret_cast<RECT*>(&m_Rect));

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

    return Application::Initialize();
}

void Win32Application::Finalize() {
    Application::Finalize();
}

void Win32Application::Tick() {
    Application::Tick();
}

void Win32Application::UpdateInputEvent() {
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
    for (int key = 0; key < static_cast<int>(VirtualKeyCode::NUM); key++) {
        g_InputManager->UpdateKeyState(static_cast<VirtualKeyCode>(key), GetAsyncKeyState(key) & 0x8000);
    }
}

LRESULT CALLBACK Win32Application::WindowProc(HWND h_wnd, UINT message, WPARAM w_param, LPARAM l_param) {
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
            sm_Quit = true;
            ClipCursor(nullptr);
            break;
        case WM_MOUSEMOVE:
            g_InputManager->UpdatePointerState({static_cast<float>(GET_X_LPARAM(l_param)), static_cast<float>(GET_Y_LPARAM(l_param))});
            break;
        case WM_MOUSEWHEEL:
            g_InputManager->UpdateWheelState(static_cast<float>(GET_WHEEL_DELTA_WPARAM(w_param)) / 120.0f);
    }
    return DefWindowProc(h_wnd, message, w_param, l_param);
}

}  // namespace Hitagi
