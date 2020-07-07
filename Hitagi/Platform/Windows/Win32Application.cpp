#include "Win32Application.hpp"

#include "InputManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <windowsx.h>

namespace Hitagi {

extern GfxConfiguration      config;
std::unique_ptr<Application> g_App = std::make_unique<Win32Application>(config);

int Win32Application::Initialize() {
    m_Logger->info("Initialize");

    // Set time period on windows
    timeBeginPeriod(1);

    // get the HINSTANCE of the Console Program
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    // this struct holds information for the window class
    WNDCLASSEX wc;

    // clear out the window class for use
    ZeroMemory(&wc, sizeof(WNDCLASSEX));

    // fill in the struct with the needed information
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = L"HitagiEngine";

    // register the window class
    RegisterClassEx(&wc);
    const std::wstring title(m_Config.appName.begin(), m_Config.appName.end());
    m_Window = CreateWindowEx(
        0,
        L"HitagiEngine",
        title.c_str(),                 // title
        WS_OVERLAPPEDWINDOW,           // Window style
        CW_USEDEFAULT, CW_USEDEFAULT,  // Position (x, y)
        m_Config.screenWidth,          // Width
        m_Config.screenHeight,         // Height
        nullptr,                       // Parent window
        nullptr,                       // Menu
        hInstance,                     // Instance handle
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
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        // translate keystroke messages into the right format
        TranslateMessage(&msg);

        // send the message to the WindowProc function
        DispatchMessage(&msg);
    }
    for (int key = 0; key < static_cast<int>(VirtualKeyCode::NUM); key++) {
        g_InputManager->UpdateState(static_cast<VirtualKeyCode>(key), GetAsyncKeyState(key) & 0x8000);
    }
}

LRESULT CALLBACK Win32Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    Win32Application* pThis;
    if (message == WM_NCCREATE) {
        pThis = static_cast<Win32Application*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);

        SetLastError(0);
        if (!SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis))) {
            if (GetLastError() != 0) return false;
        }
    } else {
        pThis = reinterpret_cast<Win32Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }
    switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            m_Quit = true;
            ClipCursor(nullptr);
            break;
        case WM_MOUSEMOVE:
            g_InputManager->UpdateState({static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam))});
            break;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

}  // namespace Hitagi
