#pragma once
#include <hitagi/core/runtime_module.hpp>

#include <vector>
#include <set>
#include <concepts>

namespace hitagi {
class Engine : public RuntimeModule {
public:
    bool Initialize() final;
    void Tick() final;
    void Finalize() final;

    template <typename T, typename... Args>
    std::pair<bool, T*> LoadModuleInplace(std::string_view name, bool force, Args&&... args) requires std::derived_from<T, RuntimeModule>;

    template <typename T>
    T* GetModule(std::string_view name);

    bool LoadModule(RuntimeModule* module, bool force = false);
    void UnloadModule(RuntimeModule* module);

private:
    std::vector<std::unique_ptr<RuntimeModule>>   m_InnerModules;
    std::pmr::set<std::unique_ptr<RuntimeModule>> m_CreatedModules;
    std::pmr::set<RuntimeModule*>                 m_OutterModules;
};

template <typename T, typename... Args>
std::pair<bool, T*> Engine::LoadModuleInplace(std::string_view name, bool force, Args&&... args) requires std::derived_from<T, RuntimeModule> {
    std::unique_ptr<RuntimeModule> module = std::make_unique<T>(std::forward<Args>(args)...);
    auto                           result = static_cast<T*>(module.get());

    if (!LoadModule(result, force)) {
        // the module will release
        return {false, nullptr};
    }
    m_CreatedModules.emplace(std::move(module));
    return {true, result};
}

template <typename T>
T* Engine::GetModule(std::string_view name) {
    if (auto iter = std::find_if(m_InnerModules.begin(), m_InnerModules.end(), [&](const auto& m) { m->GetName() == name; });
        iter != m_InnerModules.end()) {
        return static_cast<T*>((*iter)->get());
    }
    if (auto iter = std::find_if(m_OutterModules.begin(), m_OutterModules.end(), [&](auto m) { m->GetName() == name; });
        iter != m_InnerModules.end()) {
        return static_cast<T*>((*iter)->get());
    }
    return nullptr;
}

}  // namespace hitagi