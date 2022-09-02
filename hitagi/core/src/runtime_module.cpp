#include <hitagi/core/runtime_module.hpp>

namespace hitagi {

auto RuntimeModule::GetModule(std::string_view name) -> RuntimeModule* {
    auto iter = std::find_if(m_SubModules.begin(), m_SubModules.end(), [&](auto& _mod) -> bool { return _mod->GetName() == name; });
    return iter != m_SubModules.end() ? (*iter).get() : nullptr;
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