#pragma once
#include <hitagi/core/runtime_module.hpp>

#include <filesystem>
#include <memory_resource>

namespace hitagi::core {
struct AppConfig {
    std::pmr::string title   = "Hitagi Engine";
    std::pmr::string version = "v0.2.0";
    std::uint32_t    width   = 1920;
    std::uint32_t    height  = 1080;
};

class ConfigManager : public RuntimeModule {
public:
    bool Initialize() final;
    void Tick() final;
    void Finalize() final;

    bool       LoadConfig(const std::filesystem::path& path);
    AppConfig& GetConfig();

private:
    std::shared_ptr<AppConfig> m_Config;
};
}  // namespace hitagi::core

namespace hitagi {
extern core::ConfigManager* config_manager;
}  // namespace hitagi