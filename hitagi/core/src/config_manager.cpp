#include <hitagi/core/config_manager.hpp>
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/core/memory_manager.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>

namespace hitagi {
std::unique_ptr<core::ConfigManager> g_ConfigManager = std::make_unique<core::ConfigManager>();
}

namespace hitagi::core {

int ConfigManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("ConfigManager");
    m_Logger->info("Initialize...");
    LoadConfig("hitagi.json");
    return 0;
}

void ConfigManager::Tick() {}

void ConfigManager::Finalize() {
    m_Config = nullptr;
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void ConfigManager::LoadConfig(const std::filesystem::path& path) {
    m_Logger->info("lode config file: {}", path.string());
    auto buffer = g_FileIoManager->SyncOpenAndReadBinary(path);

    auto json = nlohmann::json::parse(buffer.Span<char>());

    auto new_config = std::make_shared<AppConfig>();

    new_config->title   = json["title"].get<std::string>();
    new_config->version = json["version"].get<std::string>();
    new_config->width   = json["width"].get<std::uint32_t>();
    new_config->height  = json["height"].get<std::uint32_t>();

    m_Config = std::move(new_config);
}

AppConfig& ConfigManager::GetConfig() {
    if (m_Config == nullptr) {
        m_Config = std::make_shared<AppConfig>();
    }
    return *m_Config;
}

}  // namespace hitagi::core