#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace hitagi {

bool RuntimeModule::Initialize() {
    m_Logger = spdlog::get(std::string(GetName()));
    if (m_Logger == nullptr) {
        m_Logger = spdlog::stdout_color_mt(std::string(GetName()));
    }
    m_Logger->info("Initialize {}...", GetName());
    return true;
}

void RuntimeModule::Finalize() {
    m_Logger->info("Finalize {}...", GetName());

    for (auto iter = m_SubModules.rbegin(); iter != m_SubModules.rend(); iter++) {
        (*iter)->Finalize();
    }
    m_SubModules.clear();
    m_Logger = nullptr;
}

void RuntimeModule::Tick() {
    core::Clock clock;
    clock.Start();
    for (const auto& module : m_SubModules) {
        clock.Tick();
        module->Tick();
        module->SetProfileTime(clock.DeltaTime());
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

auto RuntimeModule::LoadModule(std::unique_ptr<RuntimeModule> module) -> RuntimeModule* {
    if (!module->Initialize()) return nullptr;

    std::string_view name = module->GetName();
    if (auto iter = std::find_if(m_SubModules.begin(), m_SubModules.end(), [&](auto& _mod) -> bool { return _mod->GetName() == name; });
        iter != m_SubModules.end()) {
        (*iter)->Finalize();
        *iter = std::move(module);
        return iter->get();
    } else {
        return m_SubModules.emplace_back(std::move(module)).get();
    }
}

void RuntimeModule::UnloadModule(std::string_view name) {
    if (auto iter = std::find_if(m_SubModules.begin(), m_SubModules.end(), [&](auto& _mod) -> bool { return _mod->GetName() == name; });
        iter != m_SubModules.end()) {
        (*iter)->Finalize();
        m_SubModules.erase(iter);
    }
}

}  // namespace hitagi