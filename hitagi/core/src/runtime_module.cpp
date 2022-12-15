#include <hitagi/core/runtime_module.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <tracy/Tracy.hpp>

namespace hitagi {
RuntimeModule::RuntimeModule(std::string_view name)
    : m_Name(name),
      m_Logger(spdlog::stdout_color_mt(std::string{m_Name})) {
    m_Logger->info("Initialize...");
}

RuntimeModule::~RuntimeModule() {
    while (!m_SubModules.empty()) {
        m_SubModules.pop_back();
    }
    m_Logger->debug("Finalize...");
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

auto RuntimeModule::AddSubModule(std::unique_ptr<RuntimeModule> module) -> RuntimeModule* {
    if (module == nullptr) return nullptr;

    std::string_view name = module->GetName();
    if (auto iter = std::find_if(m_SubModules.begin(), m_SubModules.end(), [&](auto& _mod) -> bool { return _mod->GetName() == name; });
        iter != m_SubModules.end()) {
        *iter = std::move(module);
        return iter->get();
    } else {
        return m_SubModules.emplace_back(std::move(module)).get();
    }
}

void RuntimeModule::UnloadSubModule(std::string_view name) {
    std::erase_if(m_SubModules, [&](auto& _mod) -> bool { return _mod->GetName() == name; });
}

}  // namespace hitagi