#include <hitagi/application.hpp>
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/hid/input_manager.hpp>

#ifdef WIN32
#include "windows/win32_application.hpp"
#endif

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace hitagi {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AppConfig, title, version, width, height, asset_root_path);

auto load_app_config(const std::filesystem::path& config_path) -> AppConfig {
    if (config_path.empty()) return {};

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
}

void Application::Tick() {
    m_Clock.Tick();
    RuntimeModule::Tick();
}

auto Application::CreateApp(const std::filesystem::path& config_path) -> std::unique_ptr<Application> {
    return Application::CreateApp(load_app_config(config_path));
}

auto Application::CreateApp(AppConfig config) -> std::unique_ptr<Application> {
    std::unique_ptr<Application> result = nullptr;
#ifdef WIN32
    result = std::make_unique<Win32Application>(std::move(config));
#endif
    return result;
}
}  // namespace hitagi