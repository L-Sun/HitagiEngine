#pragma once
#include <hitagi/application.hpp>

#include <Windows.h>

#undef min
#undef max

namespace hitagi {
class Win32Application : public Application {
public:
    bool Initialize() final;
    void Tick() final;

    inline std::string_view GetName() const noexcept override { return "Win32Application"; }

    void InitializeWindows() final;
    void SetInputScreenPosition(unsigned x, unsigned y) final;
    void SetWindowTitle(std::string_view name) final;

    inline void* GetWindow() final { return &m_Window; };
    float        GetDpiRatio() const final;
    std::size_t  GetMemoryUsage() const final;
    inline Rect  GetWindowsRect() const final { return m_Rect; }
    inline bool  WindowSizeChanged() const final { return m_SizeChanged; }
    inline bool  IsQuit() const final { return m_Quit; }

private:
    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);

    void UpdateRect();
    void MapCursor();

    bool m_LockCursor  = false;
    bool m_SizeChanged = false;
    bool m_Quit        = false;

    Rect m_Rect;

    HWND m_Window{};
};
}  // namespace hitagi
