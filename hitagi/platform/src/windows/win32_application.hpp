#pragma once
#include <hitagi/application.hpp>

#include <Windows.h>

#undef min
#undef max

namespace hitagi {
class Win32Application : public Application {
public:
    Win32Application(AppConfig config);

    void Tick() final;

    void SetInputScreenPosition(const math::vec2u& position) final;
    void SetWindowTitle(std::string_view name) final;
    void SetCursor(Cursor cursor) final;
    void SetMousePosition(const math::vec2u& position) final;

    inline void* GetWindow() final { return &m_Window; };
    float        GetDpiRatio() const final;
    std::size_t  GetMemoryUsage() const final;
    inline Rect  GetWindowsRect() const final { return m_Rect; }
    inline bool  WindowSizeChanged() const final { return m_SizeChanged; }
    inline bool  IsQuit() const final { return m_Quit; }
    inline void  Quit() final { m_Quit = true; }

private:
    void InitializeWindows();

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
