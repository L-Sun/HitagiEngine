#pragma once
#include <windows.h>
#include <windowsx.h>
#include "BaseApplication.hpp"

namespace My {
class WindowsApplication : public BaseApplication {
public:
    WindowsApplication(GfxConfiguration& config) : BaseApplication(config) {}
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

    inline HWND GetMainWindow() const { return m_hWnd; }

protected:
    void CreateMainWindow();

private:
    // the WindowProc function prototype
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam,
                                       LPARAM lParam);

protected:
    HWND m_hWnd;
};
}  // namespace My
