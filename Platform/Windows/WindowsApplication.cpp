#include "WindowsApplication.hpp"
#include <tchar.h>

using namespace My;

int WindowsApplication::Initialize() {
    int result;

    // first call base class initialization
    result = BaseApplication::Initialize();

    if (result != 0) exit(result);

    HINSTANCE hInstance = GetModuleHandle(NULL);

    // this struct holds information for the window clas
    WNDCLASSEX wc;

    // clear out the window class for use
    ZeroMemory(&wc, sizeof(WNDCLASSEX));

    // fill in the struct wit the needed information
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = _T("GameEngine");

    // register the window class
    RegisterClassEx(&wc);

    // create the window and wuse the result as the handle
    m_hWnd = CreateWindowEx(0,
                            _T("GameEngine"),       // name of the window class
                            m_Config.appName,       // title of the window
                            WS_OVERLAPPEDWINDOW,    // window style
                            CW_USEDEFAULT,          // x-position of the window
                            CW_USEDEFAULT,          // y-position of the window
                            m_Config.screenWidth,   // width of the window
                            m_Config.screenHeight,  // height of the window
                            NULL,                   // we have no parent window
                            NULL,                   // we are not using menus
                            hInstance,              // application handle
                            this                    // pass pointer to current
    );
    // display the window on the screen
    ShowWindow(m_hWnd, SW_SHOW);

    return result;
}

void WindowsApplication::Finalize() {}

void WindowsApplication::Tick() {
    // this struct holds windows event messages
    MSG msg;

    // we use PeekMessage instead of GetMessage here
    // because we should not block the thread at any where
    // except the engine execution driver module
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        // translate keystroke message into the right formate
        TranslateMessage(&msg);
        // send the message to the WindowProc function
        DispatchMessage(&msg);
    }
}

// this is the main message handler for the program
LRESULT CALLBACK WindowsApplication::WindowProc(HWND hWnd, UINT message,
                                                WPARAM wParam, LPARAM lParam) {
    WindowsApplication* pThis;
    if (message == WM_NCCREATE) {
        pThis = static_cast<WindowsApplication*>(
            reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);

        SetLastError(0);
        if (!SetWindowLongPtr(hWnd, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(pThis))) {
            if (GetLastError() != 0) {
                return FALSE;
            }
        }
    } else {
        pThis = reinterpret_cast<WindowsApplication*>(
            GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    // sort through and find what code to run for the message given

    switch (message) {
        case WM_KEYDOWN:
            // we will replace this with input manager
            m_bQuit = true;
            break;
        case WM_DESTROY:
            // cloase the application entirely
            PostQuitMessage(0);
            m_bQuit = true;
            return 0;
        default:
            break;
    }

    // Handle any message the switch statement didn't
    return DefWindowProc(hWnd, message, wParam, lParam);
}