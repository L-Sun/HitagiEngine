#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <list>
#include <vector>
#include <unordered_map>

namespace spdlog {
class logger;
}

namespace hitagi {
class RuntimeModule {
public:
    RuntimeModule(std::string_view name);
    virtual ~RuntimeModule();
    virtual void Tick();

    inline auto GetName() const noexcept -> std::string_view { return m_Name; };

    auto         GetSubModule(std::string_view name) -> RuntimeModule*;
    auto         GetSubModules() const noexcept -> std::pmr::vector<RuntimeModule*>;
    virtual auto AddSubModule(std::unique_ptr<RuntimeModule> module, RuntimeModule* after = nullptr) -> RuntimeModule*;
    virtual void UnloadSubModule(std::string_view name);

    static auto GetModule(std::string_view name) -> RuntimeModule*;

protected:
    std::pmr::string                               m_Name;
    std::shared_ptr<spdlog::logger>                m_Logger;
    std::pmr::list<std::unique_ptr<RuntimeModule>> m_SubModules;

    static std::unordered_map<std::string, RuntimeModule*> sm_AllModules;
};

}  // namespace hitagi
