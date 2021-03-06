#pragma once
#include "Application.hpp"
#include <Windows.h>

namespace Hitagi {
class Win32Application : public Application {
public:
    using Application::Application;
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    void  UpdateInputEvent() final;
    void* GetWindow() final { return &m_Window; }

private:
    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);

    bool m_LockCursor = false;
    HWND m_Window;
};
}  // namespace Hitagi
