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

    void* GetWindow() final { return &m_Window; }
    float GetDpiRatio() final;

private:
    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);

    void UpdateRect();
    void MapCursor();

    bool m_LockCursor = false;
    HWND m_Window{};
};
}  // namespace Hitagi
