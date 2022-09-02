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
    virtual ~RuntimeModule()  = default;
    virtual bool Initialize() = 0;
    virtual void Finalize();
    virtual void Tick();

    virtual std::string_view GetName() const = 0;

    RuntimeModule*         GetModule(std::string_view name);
    virtual RuntimeModule* LoadModule(std::unique_ptr<RuntimeModule> module);
    virtual void           UnloadModule(std::string_view name);

protected:
    std::shared_ptr<spdlog::logger>                m_Logger;
    std::pmr::list<std::unique_ptr<RuntimeModule>> m_SubModules;
};

}  // namespace hitagi
