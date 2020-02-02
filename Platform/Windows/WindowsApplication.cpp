
#include "WindowsApplication.hpp"
#include <tchar.h>

using namespace My;

void WindowsApplication::CreateMainWindow() {
    // get the HINSTANCE of the Console Program
    HINSTANCE hInstance = GetModuleHandle(NULL);

    // this struct holds information for the window class
    WNDCLASSEX wc;

    // clear out the window class for use
    ZeroMemory(&wc, sizeof(WNDCLASSEX));

    // fill in the struct with the needed information
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = _T("GameEngineFromScratch");

    // register the window class
    RegisterClassEx(&wc);

    // create the window and use the result as the handle
    m_hWnd =
        CreateWindowEx(0,
                       _T("GameEngineFromScratch"),  // name of the window class
                       m_Config.appName.c_str(),     // title of the window
                       WS_OVERLAPPEDWINDOW,          // window style
                       CW_USEDEFAULT,                // x-position of the window
                       CW_USEDEFAULT,                // y-position of the window
                       m_Config.screenWidth,         // width of the window
                       m_Config.screenHeight,        // height of the window
                       NULL,       // we have no parent window, NULL
                       NULL,       // we aren't using menus, NULL
                       hInstance,  // application handle
                       this);      // pass pointer to current object

    // display the window on the screen
    ShowWindow(m_hWnd, SW_SHOW);
}

int WindowsApplication::Initialize() {
    int result;

    CreateMainWindow();

    m_hDc = GetDC(m_hWnd);

    // call base class initialization
    result = BaseApplication::Initialize();

    if (result != 0) exit(result);

    return result;
}

void WindowsApplication::Finalize() {
    ReleaseDC(m_hWnd, m_hDc);

    BaseApplication::Finalize();
}

void WindowsApplication::Tick() {
    BaseApplication::Tick();
    std::ostringstream ss;
    ss << "FPS: " << m_FPS;
    SetWindowTextA(m_hWnd, ss.str().c_str());
    // this struct holds Windows event messages
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
            if (GetLastError() != 0) return FALSE;
        }
    } else {
        pThis = reinterpret_cast<WindowsApplication*>(
            GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    // sort through and find what code to run for the message given
    switch (message) {
        case WM_KEYUP: {
            switch (wParam) {
                case VK_LEFT:
                    g_pInputManager->LeftArrowKeyUp();
                    break;
                case VK_RIGHT:
                    g_pInputManager->RightArrowKeyUp();
                    break;
                case VK_UP:
                    g_pInputManager->UpArrowKeyUp();
                    break;
                case VK_DOWN:
                    g_pInputManager->DownArrowKeyUp();
                    break;
                case 0x43:
                    g_pInputManager->CKeyUp();
                    break;
                case 0x44:
                    g_pInputManager->DebugKeyUp();
                case 0x52:
                    g_pInputManager->ResetKeyUp();
                    break;
                default:
                    break;
            }
        } break;
        case WM_KEYDOWN: {
            switch (wParam) {
                case VK_LEFT:
                    g_pInputManager->LeftArrowKeyDown();
                    break;
                case VK_RIGHT:
                    g_pInputManager->RightArrowKeyDown();
                    break;
                case VK_UP:
                    g_pInputManager->UpArrowKeyDown();
                    break;
                case VK_DOWN:
                    g_pInputManager->DownArrowKeyDown();
                    break;
                case 0x43:
                    g_pInputManager->CKeyDown();
                    break;
                case 0x44:
                    g_pInputManager->DebugKeyDown();
                    break;
                case 0x52:
                    g_pInputManager->ResetKeyDown();
                    break;
                default:
                    break;
            }
        } break;
        case WM_DESTROY: {
            // close the application entirely
            PostQuitMessage(0);
            m_bQuit = true;
        }
    }

    // Handle any messages the switch statement didn't
    return DefWindowProc(hWnd, message, wParam, lParam);
}