#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/math/vector.hpp>

#include <vector>
#include <filesystem>

namespace hitagi {
enum struct Cursor : std::uint8_t {
    None,
    Arrow,
    TextInput,
    ResizeAll,
    ResizeEW,
    ResizeNS,
    ResizeNESW,
    ResizeNWSE,
    Hand,
    Forbid,
};

struct AppConfig {
    std::pmr::string      title           = "Hitagi Engine";
    std::pmr::string      version         = "v0.2.0";
    std::uint32_t         width           = 800;
    std::uint32_t         height          = 800;
    std::filesystem::path asset_root_path = "assets";
};

class Application : public RuntimeModule {
public:
    struct Rect {
        std::uint32_t left = 0, top = 0, right = 0, bottom = 0;
    };
    Application(AppConfig config);
    ~Application() override;

    static auto CreateApp(AppConfig config) -> std::unique_ptr<Application>;
    static auto CreateApp(const std::filesystem::path& config_path = "hitagi.json") -> std::unique_ptr<Application>;

    void Tick() override;

    static std::unique_ptr<Application> CreateApp();

    virtual void SetInputScreenPosition(const math::vec2u& position) = 0;
    virtual void SetWindowTitle(std::string_view name)               = 0;
    virtual void SetCursor(Cursor cursor)                            = 0;
    virtual void SetMousePosition(const math::vec2u& position)       = 0;
    virtual void SetInputScreenPosition(unsigned x, unsigned y)      = 0;

    virtual void*       GetWindow()               = 0;
    virtual float       GetDpiRatio() const       = 0;
    virtual std::size_t GetMemoryUsage() const    = 0;
    virtual Rect        GetWindowsRect() const    = 0;
    virtual bool        WindowSizeChanged() const = 0;
    virtual bool        IsQuit() const            = 0;
    virtual void        Quit()                    = 0;

    inline auto GetConfig() const noexcept { return m_Config; }

protected:
    core::Clock m_Clock;
    AppConfig   m_Config;
};
extern Application* app;
}  // namespace hitagi
