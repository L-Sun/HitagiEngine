module;

#include "interop/tracy_macros.hpp"

export module core:runtime_module;
import interop.tracy;
import interop.spdlog;

import std;
import utils;

export namespace hitagi::core {

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

protected:
    // Destroys sub-modules in reverse insertion order (what the destructor does).
    // Lets a derived module end its sub-modules' lifetime before its own members.
    void UnloadAllSubModules() noexcept;

    std::pmr::string                               m_Name;
    std::shared_ptr<spdlog::logger>                m_Logger;
    std::pmr::list<std::unique_ptr<RuntimeModule>> m_SubModules;
};

}  // namespace hitagi::core

namespace hitagi::core {

RuntimeModule::RuntimeModule(std::string_view name)
    : m_Name(name), m_Logger(utils::try_create_logger(name)) {
    const auto message = std::format("Initialize {}", m_Name);
    ZoneScoped;
    ZoneName(message.data(), message.size());
    m_Logger->info("Initialize...");
}

RuntimeModule::~RuntimeModule() {
    UnloadAllSubModules();
    m_Logger->info("Finalize {}", m_Name);
}

void RuntimeModule::UnloadAllSubModules() noexcept {
    while (!m_SubModules.empty()) {
        ZoneScoped;
        const auto& sub_module = m_SubModules.back();
        const auto  message    = std::format("Finalize {}", sub_module->GetName());
        ZoneName(message.data(), message.size());
        m_SubModules.pop_back();
    }
}

void RuntimeModule::Tick() {
    for (const auto& sub_module : m_SubModules) {
        ZoneScoped;
        ZoneName(sub_module->GetName().data(), sub_module->GetName().size());
        sub_module->Tick();
    }
}

auto RuntimeModule::GetSubModule(std::string_view name) -> RuntimeModule* {
    auto iter = std::find_if(m_SubModules.begin(), m_SubModules.end(), [&](auto& _mod) -> bool { return _mod->GetName() == name; });
    return iter != m_SubModules.end() ? (*iter).get() : nullptr;
}

auto RuntimeModule::GetSubModules() const noexcept -> std::pmr::vector<RuntimeModule*> {
    std::pmr::vector<RuntimeModule*> result;
    std::transform(m_SubModules.begin(), m_SubModules.end(), std::back_inserter(result), [](const auto& submodule) { return submodule.get(); });
    return result;
}

auto RuntimeModule::AddSubModule(std::unique_ptr<RuntimeModule> module, RuntimeModule* after) -> RuntimeModule* {
    if (module == nullptr) return nullptr;

    if (after) {
        const auto iter = std::find_if(m_SubModules.begin(), m_SubModules.end(), [=](auto& _mod) -> bool { return _mod.get() == after; });
        if (iter == m_SubModules.end()) {
            const auto error_message = std::format("Module {} does not exist in current module({})", after->GetName(), GetName());
            m_Logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
        return m_SubModules.insert(std::next(iter), std::move(module))->get();
    } else {
        m_SubModules.push_back(std::move(module));
        return m_SubModules.back().get();
    }
}

void RuntimeModule::UnloadSubModule(std::string_view name) {
    std::erase_if(m_SubModules, [&](auto& _mod) -> bool { return _mod->GetName() == name; });
}

}  // namespace hitagi::core
