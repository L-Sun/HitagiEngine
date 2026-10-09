export module app:application;
import interop.spdlog;

import std;
import utils;
import math;
import core;
import hid;

export namespace hitagi {
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
    bool                  maximized       = false;
    std::filesystem::path asset_root_path = "assets";
    std::pmr::string      gfx_backend     = "Vulkan";
    std::pmr::string      log_level       = "info";
    bool                  headless        = false;
};

class Application : public core::RuntimeModule {
public:
    struct Rect {
        std::uint32_t left = 0, top = 0, right = 0, bottom = 0;
    };
    Application(AppConfig config);
    ~Application() override;

    static auto CreateApp(AppConfig config = {}) -> std::unique_ptr<Application>;

    void Tick() override;

    virtual void SetInputScreenPosition(const math::vec2u& position)     = 0;
    virtual void SetWindowTitle(std::string_view name)                   = 0;
    virtual void SetCursor(Cursor cursor)                                = 0;
    virtual void SetMousePosition(const math::vec2u& position)           = 0;
    virtual void ResizeWindow(std::uint32_t width, std::uint32_t height) = 0;

    virtual auto GetWindow() const -> utils::Window    = 0;
    virtual auto GetDpiRatio() const -> float          = 0;
    virtual auto GetMemoryUsage() const -> std::size_t = 0;
    virtual auto GetWindowRect() const -> Rect         = 0;
    virtual bool WindowSizeChanged() const             = 0;
    virtual bool WindowsMinimized() const              = 0;
    virtual bool WindowMaximized() const               = 0;
    virtual bool IsQuit() const                        = 0;
    virtual void Quit()                                = 0;

    auto GetWindowWidth() const -> std::uint32_t;
    auto GetWindowHeight() const -> std::uint32_t;

    inline auto GetConfig() const noexcept { return m_Config; }

    inline auto& GetInputManager() const noexcept { return *m_InputManager; }

protected:
    core::Clock        m_Clock;
    AppConfig          m_Config;
    hid::InputManager* m_InputManager;
};
}  // namespace hitagi

namespace hitagi {
Application::Application(AppConfig config)
    : core::RuntimeModule(config.title),
      m_Config(std::move(config)) {
    spdlog::set_level(spdlog::level::from_str(m_Config.log_level.data()));

    m_InputManager = static_cast<hid::InputManager*>(AddSubModule(std::make_unique<hid::InputManager>()));
    m_Clock.Start();
}

Application::~Application() = default;

void Application::Tick() {
    m_Clock.Tick();
    m_Config.maximized = WindowMaximized();
    if (WindowSizeChanged() && !WindowsMinimized() && !m_Config.maximized) {
        m_Config.width  = GetWindowWidth();
        m_Config.height = GetWindowHeight();
    }
    core::RuntimeModule::Tick();
}

auto Application::GetWindowWidth() const -> std::uint32_t {
    const auto rect = GetWindowRect();
    return rect.right - rect.left;
}

auto Application::GetWindowHeight() const -> std::uint32_t {
    const auto rect = GetWindowRect();
    return rect.bottom - rect.top;
}

}  // namespace hitagi
