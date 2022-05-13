#pragma once
#include <hitagi/core/runtime_module.hpp>

#include <filesystem>
#include <memory_resource>

namespace hitagi::core {
struct AppConfig {
    using allocator_type = std::pmr::polymorphic_allocator<>;
    AppConfig(allocator_type = {});

    std::pmr::string title   = "Hitagi Engine";
    std::pmr::string version = "v0.2.0";
    std::uint32_t    width   = 1920;
    std::uint32_t    height  = 1080;
};

class ConfigManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Tick() final;
    void Finalize() final;

    void       LoadConfig(const std::filesystem::path& path);
    AppConfig& GetConfig() { return *m_Config; };

private:
    std::shared_ptr<AppConfig> m_Config;
};
}  // namespace hitagi::core

namespace hitagi {
extern std::unique_ptr<core::ConfigManager> g_ConfigManager;
}