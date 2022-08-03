#include <hitagi/core/config_manager.hpp>
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/core/memory_manager.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>

namespace hitagi {
core::ConfigManager* config_manager = nullptr;
}

namespace hitagi::core {

bool ConfigManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("ConfigManager");
    m_Logger->info("Initialize...");
    if (!LoadConfig("hitagi.json"))
        return false;
    return true;
}

void ConfigManager::Tick() {}

void ConfigManager::Finalize() {
    SaveConfig("hitagi.json");
    m_Config = nullptr;
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

bool ConfigManager::LoadConfig(const std::filesystem::path& path) {
    m_Logger->info("lode config file: {}", path.string());
    auto& buffer = file_io_manager->SyncOpenAndReadBinary(path);
    if (buffer.Empty())
        return false;

    try {
        auto json = nlohmann::json::parse(buffer.Span<char>());

        auto new_config = std::make_shared<AppConfig>();

        new_config->title   = json["title"].get<std::string>();
        new_config->version = json["version"].get<std::string>();
        new_config->width   = json["width"].get<std::uint32_t>();
        new_config->height  = json["height"].get<std::uint32_t>();

        m_Config = std::move(new_config);

    } catch (...) {
        return false;
    }

    return true;
}

void ConfigManager::SaveConfig(const std::filesystem::path& path) {
    m_Logger->info("save config to file: {}", path.string());

    auto json = nlohmann::json();

    json["title"]   = m_Config->title;
    json["version"] = m_Config->version;
    json["width"]   = m_Config->width;
    json["height"]  = m_Config->height;

    auto content = json.dump();
    file_io_manager->SaveBuffer(core::Buffer(content.data(), content.size()), path);
}

AppConfig& ConfigManager::GetConfig() {
    if (m_Config == nullptr) {
        m_Config = std::make_shared<AppConfig>();
    }
    return *m_Config;
}

}  // namespace hitagi::core