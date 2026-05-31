module;

#include <spdlog/spdlog.h>

module app;
import std;

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

auto Application::CreateApp(AppConfig config) -> std::unique_ptr<Application> {
    std::unique_ptr<Application> result = nullptr;
#if defined(_WIN32)
    result = std::make_unique<Win32Application>(std::move(config));
#else
    result = std::make_unique<SDL3Application>(std::move(config));
#endif
    return result;
}

}  // namespace hitagi
