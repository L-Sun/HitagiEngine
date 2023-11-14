
#include <hitagi/application.hpp>
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/hid/input_manager.hpp>

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <fstream>

namespace hitagi {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AppConfig, title, version, width, height, asset_root_path, gfx_backend, log_level);

auto load_app_config(const std::filesystem::path& config_path) -> std::optional<AppConfig> {
    if (config_path.empty() || !std::filesystem::exists(config_path))
        return std::nullopt;

    nlohmann::json json;
    if (file_io_manager) {
        json = nlohmann::json::parse(file_io_manager->SyncOpenAndReadBinary(config_path).Str());
    } else {
        std::ifstream ifs(config_path);
        json = nlohmann::json::parse(ifs);
    }
    return json.get<AppConfig>();
}

Application::Application(AppConfig config)
    : RuntimeModule(config.title),
      m_Config(std::move(config)) {
    spdlog::set_level(spdlog::level::from_str(m_Config.log_level.data()));

    if (!input_manager) {
        input_manager = static_cast<decltype(input_manager)>(AddSubModule(std::make_unique<hid::InputManager>()));
    }
    m_Clock.Start();
}

Application::~Application() {
    if (file_io_manager) {
        std::filesystem::path path = "hitagi.json";
        m_Logger->info("save config to file: {}", path.string());

        nlohmann::json json = m_Config;

        auto content = json.dump(4);
        file_io_manager->SaveBuffer(core::Buffer(content.size(), reinterpret_cast<const std::byte*>(content.data())), path);
    }
    for (const auto& sub_module : m_SubModules) {
        if (sub_module.get() == input_manager) {
            input_manager = nullptr;
        }
    }
}

void Application::Tick() {
    m_Clock.Tick();
    RuntimeModule::Tick();
}

auto Application::CreateApp(const std::filesystem::path& config_path) -> std::unique_ptr<Application> {
    auto config = load_app_config(config_path);
    return Application::CreateApp(config.has_value() ? config.value() : AppConfig{});
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

#if defined(_WIN32)
#include "windows/win32_application.hpp"
#elif defined(__linux__)
#include "sdl2/sdl2_application.hpp"
#endif

namespace hitagi {
auto Application::CreateApp(AppConfig config) -> std::unique_ptr<Application> {
    std::unique_ptr<Application> result = nullptr;
#if defined(_WIN32)
    result = std::make_unique<Win32Application>(std::move(config));
#elif defined(__linux__)
    result = std::make_unique<SDL2Application>(std::move(config));
#endif
    return result;
}
}  // namespace hitagi