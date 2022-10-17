#pragma once
#include <memory>
#include <string_view>
#include <list>

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

    RuntimeModule*         GetModule(std::string_view name);
    virtual RuntimeModule* LoadModule(std::unique_ptr<RuntimeModule> module);
    virtual void           UnloadModule(std::string_view name);

    inline auto GetLogger() const noexcept { return m_Logger; }

protected:
    std::shared_ptr<spdlog::logger>                m_Logger;
    std::pmr::list<std::unique_ptr<RuntimeModule>> m_SubModules;
};

}  // namespace hitagi
