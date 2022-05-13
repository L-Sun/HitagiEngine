#include <hitagi/core/config_manager.hpp>
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/core/memory_manager.hpp>
#include "yaml-cpp/node/parse.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <yaml-cpp/yaml.h>

namespace hitagi {
std::unique_ptr<core::ConfigManager> g_ConfigManager = std::make_unique<core::ConfigManager>();
}

namespace hitagi::core {
AppConfig::AppConfig(allocator_type alloc) {}

int ConfigManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("ConfigManager");
    m_Logger->info("Initialize...");

    m_Config = std::allocate_shared<AppConfig>(g_MemoryManager->GetAllocator<AppConfig>());

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
    auto       buffer = g_FileIoManager->SyncOpenAndReadBinary(path);
    YAML::Node node;
    try {
        node = YAML::Load(reinterpret_cast<const char*>(buffer.GetData()));
    } catch (YAML::ParserException ex) {
        m_Logger->error(ex.what());
    }
    auto& config = *m_Config;

    config.title   = node["title"].as<std::string>();
    config.version = node["version"].as<std::string>();
    config.width   = node["width"].as<std::uint32_t>();
    config.height  = node["width"].as<std::uint32_t>();
}

}  // namespace hitagi::core