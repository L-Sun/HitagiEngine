#include <hitagi/application.hpp>

#include <SDL2/SDL.h>

namespace hitagi {

class SDL2Application final : public Application {
public:
    SDL2Application(AppConfig config);
    ~SDL2Application() final;

    void Tick() final;

    void SetInputScreenPosition(const math::vec2u& position) final;
    void SetWindowTitle(std::string_view name) final;
    void SetCursor(Cursor cursor) final;
    void SetMousePosition(const math::vec2u& position) final;
    void ResizeWindow(std::uint32_t width, std::uint32_t height) final;

    auto GetWindow() const -> utils::Window final;
    auto GetDpiRatio() const -> float final;
    auto GetMemoryUsage() const -> std::size_t final;
    auto GetWindowRect() const -> Rect final;

    inline bool WindowSizeChanged() const final { return m_SizeChanged; }
    inline bool WindowsMinimized() const final { return m_Minimized; };
    inline void Quit() final { m_Quit = true; }
    inline bool IsQuit() const final { return m_Quit; }

private:
    SDL_Window* m_Window = nullptr;

    bool m_Quit        = false;
    bool m_SizeChanged = false;
    bool m_Minimized   = false;
};

}  // namespace hitagi