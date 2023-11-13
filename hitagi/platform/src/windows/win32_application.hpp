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
    void ResizeWindow(std::uint32_t width, std::uint32_t height) final;

    auto        GetWindow() const -> utils::Window final;
    inline auto GetWindowRect() const -> Rect final { return m_Rect; }
    inline bool WindowSizeChanged() const final { return m_SizeChanged; }
    inline bool WindowsMinimized() const final { return m_Minimized; };
    inline bool IsQuit() const final { return m_Quit; }
    inline void Quit() final { m_Quit = true; }

    auto GetDpiRatio() const -> float final;
    auto GetMemoryUsage() const -> std::size_t final;

private:
    void InitializeWindows();

    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);

    void UpdateRect();
    void MapCursor();

    bool m_LockCursor  = false;
    bool m_SizeChanged = false;
    bool m_Minimized   = false;
    bool m_Quit        = false;

    Rect m_Rect;

    mutable HWND m_Window{};
};
}  // namespace hitagi
