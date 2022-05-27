#pragma once
#include <hitagi/application.hpp>

#include <Windows.h>

#undef min
#undef max

namespace hitagi {
class Win32Application : public Application {
public:
    using Application::Application;
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    void* GetWindow() final { return &m_Window; }
    void  InitializeWindows() final;
    void  SetInputScreenPosition(unsigned x, unsigned y) final;
    float GetDpiRatio() final;

private:
    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);

    void UpdateRect();
    void MapCursor();

    bool m_LockCursor = false;
    HWND m_Window{};
};
}  // namespace hitagi
