#include <iostream>
#include <tchar.h>
#include "OpenGLApplication.hpp"
#include "OpenGL/OpenGLGraphicsManager.hpp"
#include "glad/glad_wgl.h"

using namespace My;

static LRESULT CALLBACK TmpWndProc(HWND hWnd, UINT uiMsg, WPARAM wParam,
                                   LPARAM lParam) {
    switch (uiMsg) {
        case WM_CLOSE:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hWnd, uiMsg, wParam, lParam);
    }

    return 0;
}

int OpenGLApplication::Initialize() {
    int  result;
    auto colorBits = m_Config.redBits + m_Config.greenBits + m_Config.blueBits;

    DWORD      Style = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    WNDCLASSEX WndClassEx;
    memset(&WndClassEx, 0, sizeof(WNDCLASSEX));

    HINSTANCE hInstance      = GetModuleHandle(NULL);
    WndClassEx.cbSize        = sizeof(WNDCLASSEX);
    WndClassEx.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WndClassEx.lpfnWndProc   = TmpWndProc;
    WndClassEx.hInstance     = hInstance;
    WndClassEx.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    WndClassEx.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
    WndClassEx.hCursor       = LoadCursor(NULL, IDC_ARROW);
    WndClassEx.lpszClassName = _T("InitWindow");

    RegisterClassEx(&WndClassEx);
    HWND TemphWnd =
        CreateWindowEx(WS_EX_APPWINDOW,
                       WndClassEx.lpszClassName,  // name of the window class
                       _T("InitWindow"),          // title of the window
                       Style,                     // window style
                       0,                         // x-position of the window
                       0,                         // y-position of the window
                       CW_USEDEFAULT,             // width of the window
                       CW_USEDEFAULT,             // height of the window
                       NULL,       // we have no parent window, NULL
                       NULL,       // we aren't using menus, NULL
                       hInstance,  // application handle
                       NULL        // pass pointer to current object
        );

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
    pfd.nSize      = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = colorBits;
    pfd.cRedBits   = m_Config.redBits;
    pfd.cGreenBits = m_Config.greenBits;
    pfd.cBlueBits  = m_Config.blueBits;
    pfd.cAlphaBits = m_Config.alphaBits;
    pfd.cDepthBits = m_Config.depthBits;
    pfd.cStencilBits = m_Config.stencilBits;
    pfd.iLayerType   = PFD_MAIN_PLANE;

    HDC TemphDC = GetDC(TemphWnd);
    // Set a temporary default pixel formate
    int nPixelFormat = ChoosePixelFormat(TemphDC, &pfd);
    if (nPixelFormat == 0) return -1;

    result = SetPixelFormat(TemphDC, nPixelFormat, &pfd);
    if (result != 1) return result;

    // Create a temporary rendering context.
    m_RenderContext = wglCreateContext(TemphDC);
    if (!m_RenderContext) {
        std::cout << "wglCreateContext failed!" << std::endl;
        return -1;
    }

    // Set the temporary rendering context as the current rendering
    // context for this window.
    result = wglMakeCurrent(TemphDC, m_RenderContext);
    if (result != 1) return result;

    if (!gladLoadWGL(TemphDC)) {
        std::cout << "WGL initialize failed!" << std::endl;
        return -1;
    } else {
        result = 0;
        std::cout << "WGL initialize finished!" << std::endl;
    }

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(m_RenderContext);
    ReleaseDC(TemphWnd, TemphDC);
    DestroyWindow(TemphWnd);

    // now initialize our application window
    WindowsApplication::CreateMainWindow();

    m_hDC = GetDC(m_hWnd);

    // now we try init OpenGL Core profile context
    if (GLAD_WGL_ARB_pixel_format && GLAD_WGL_ARB_multisample &&
        GLAD_WGL_ARB_create_context) {
        // enable MSAA
        const int attributes[] = {
            // clang-format off
			WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
			WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
			WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
			WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
			WGL_COLOR_BITS_ARB,     (int)colorBits,
			WGL_RED_BITS_ARB,		(int)m_Config.redBits,
			WGL_GREEN_BITS_ARB,		(int)m_Config.greenBits,
			WGL_BLUE_BITS_ARB,		(int)m_Config.blueBits,
			WGL_ALPHA_BITS_ARB,		(int)m_Config.alphaBits,
			WGL_DEPTH_BITS_ARB,     (int)m_Config.depthBits,
			WGL_STENCIL_BITS_ARB,   (int)m_Config.stencilBits,
			WGL_SAMPLE_BUFFERS_ARB, 1,  // 4x MSAA
			WGL_SAMPLES_ARB,        4,  // 4x MSAA
			0
            // clang-format on
        };
        UINT numFormats;

        if (FAILED(wglChoosePixelFormatARB(m_hDC, attributes, nullptr, 1,
                                           &nPixelFormat, &numFormats)) ||
            numFormats == 0) {
            std::cout << "wglChoosePixelFormateARB failed!" << std::endl;
            return -1;
        }

        result = SetPixelFormat(m_hDC, nPixelFormat, &pfd);
        if (result != 1) {
            return result;
        }

        const int context_attributes[] = {
            // clang-format off
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
			WGL_CONTEXT_MINOR_VERSION_ARB, 3,
			WGL_CONTEXT_FLAGS_ARB,         WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
			WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB, 
			0
            // clang-format on
        };
        m_RenderContext =
            wglCreateContextAttribsARB(m_hDC, 0, context_attributes);

        if (!m_RenderContext) {
            std::cout << "wglCreateContextAttributeARB failed!" << std::endl;
            return -1;
        }

        result = wglMakeCurrent(m_hDC, m_RenderContext);

        if (result != 1) {
            return result;
        }
        result = 0;  // we use 0 as success while OpenGL use 1, so covert it
    } else {
        // Set pixel format.
        int nPixelFormat = ChoosePixelFormat(m_hDC, &pfd);
        if (nPixelFormat == 0) return -1;

        result = SetPixelFormat(m_hDC, nPixelFormat, &pfd);
        if (result != 1) return result;

        // Create rendering context
        m_RenderContext = wglCreateContext(m_hDC);
        if (!m_RenderContext) {
            std::cout << "wglCreateContext failed!" << std::endl;
            return -1;
        }

        // Set the rendering context as the current rendering context for this
        // window.
        result = wglMakeCurrent(m_hDC, m_RenderContext);
        if (result != 1) {
            return result;
        }
    }

    result = BaseApplication::Initialize();

    if (result) {
        printf("Windows Application initialize failed!");
        return result;
    }

    return result;
}

void OpenGLApplication::Finalize() {
    if (m_RenderContext) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(m_RenderContext);
        m_RenderContext = 0;
    }
    WindowsApplication::Finalize();
}
void OpenGLApplication::Tick() {
    WindowsApplication::Tick();
    g_pGraphicsManager->Clear();
    g_pGraphicsManager->Draw();
    // Present the back buffer to the screen since rendering is complete
    SwapBuffers(m_hDC);
}
