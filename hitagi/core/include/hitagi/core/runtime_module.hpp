#pragma once
#include <memory>
#include <string_view>
#include <list>
#include <chrono>

namespace spdlog {
class logger;
}

namespace hitagi {
class RuntimeModule {
public:
    virtual ~RuntimeModule() = default;
    virtual bool Initialize();
    virtual void Finalize();
    virtual void Tick();

    virtual std::string_view GetName() const = 0;

    RuntimeModule*         GetSubModule(std::string_view name);
    auto                   GetSubModules() const noexcept -> std::pmr::vector<RuntimeModule*>;
    virtual RuntimeModule* LoadModule(std::unique_ptr<RuntimeModule> module);
    virtual void           UnloadModule(std::string_view name);

    inline auto GetLogger() const noexcept { return m_Logger; }

protected:
    std::shared_ptr<spdlog::logger>                m_Logger;
    std::pmr::list<std::unique_ptr<RuntimeModule>> m_SubModules;
};

}  // namespace hitagi
