export module ecs:system_manager;
import std;
import utils;

import :schedule;

namespace hitagi::ecs::detail {
struct SystemCallbacks {
    std::function<void()>          on_create;
    std::function<void()>          on_enable;
    std::function<void(Schedule&)> configure_schedule;
    std::function<void()>          on_disable;
    std::function<void()>          on_destroy;
};
}  // namespace hitagi::ecs::detail

export namespace hitagi::ecs {

// Registry of already-bound callbacks. It neither owns nor calls a World.
class SystemManager {
public:
    SystemManager() = default;
    ~SystemManager() { Shutdown(); }
    SystemManager(const SystemManager&)            = delete;
    SystemManager& operator=(const SystemManager&) = delete;

    template <typename... Systems>
    void Enable() { (EnableOne(utils::TypeID::Create<Systems>()), ...); }
    template <typename... Systems>
    void Disable() { (DisableOne(utils::TypeID::Create<Systems>()), ...); }
    template <typename... Systems>
    void Unregister() { (UnregisterOne(utils::TypeID::Create<Systems>()), ...); }

private:
    friend class World;
    void Register(utils::TypeID id, detail::SystemCallbacks callbacks, std::shared_ptr<void> state);
    void Update(Schedule& schedule);
    void Shutdown();
    void EnableOne(utils::TypeID id);
    void DisableOne(utils::TypeID id);
    void UnregisterOne(utils::TypeID id);

    struct Entry {
        detail::SystemCallbacks callbacks;
        std::shared_ptr<void>   state;
        bool                    enabled = false;
    };
    std::pmr::unordered_map<utils::TypeID, Entry> m_Systems;
    std::vector<utils::TypeID>                    m_RegistrationOrder;
    std::uint64_t                                 m_Revision     = 0;
    bool                                          m_ShuttingDown = false;
};

void SystemManager::Register(utils::TypeID id, detail::SystemCallbacks callbacks, std::shared_ptr<void> state) {
    if (m_ShuttingDown) return;
    const auto [entry, inserted] = m_Systems.try_emplace(id, std::move(callbacks), std::move(state));
    if (inserted) {
        m_RegistrationOrder.emplace_back(id);
        const auto installed = entry->second.state;
        // Copy before invoking: a callback may register or unregister systems.
        const auto on_create = entry->second.callbacks.on_create;
        try {
            if (on_create) on_create();
            EnableOne(id);
        } catch (...) {
            const auto current = m_Systems.find(id);
            if (current != m_Systems.end() && current->second.state == installed) {
                m_Systems.erase(current);
                std::erase(m_RegistrationOrder, id);
                ++m_Revision;
            }
            throw;
        }
    } else {
        EnableOne(id);
    }
}

void SystemManager::EnableOne(utils::TypeID id) {
    const auto entry = m_Systems.find(id);
    if (entry == m_Systems.end() || entry->second.enabled) return;
    entry->second.enabled = true;
    ++m_Revision;
    const auto state    = entry->second.state;
    const auto callback = entry->second.callbacks.on_enable;
    try {
        if (callback) callback();
    } catch (...) {
        const auto current = m_Systems.find(id);
        if (current != m_Systems.end() && current->second.state == state) current->second.enabled = false;
        throw;
    }
}

void SystemManager::DisableOne(utils::TypeID id) {
    const auto entry = m_Systems.find(id);
    if (entry == m_Systems.end() || !entry->second.enabled) return;
    entry->second.enabled = false;
    ++m_Revision;
    const auto callback = entry->second.callbacks.on_disable;
    if (callback) callback();
}

void SystemManager::UnregisterOne(utils::TypeID id) {
    const auto entry = m_Systems.find(id);
    if (entry == m_Systems.end()) return;
    auto removed = std::move(entry->second);
    m_Systems.erase(entry);
    std::erase(m_RegistrationOrder, id);
    ++m_Revision;
    if (removed.enabled && removed.callbacks.on_disable) removed.callbacks.on_disable();
    if (removed.callbacks.on_destroy) removed.callbacks.on_destroy();
}

void SystemManager::Update(Schedule& schedule) {
    // Freeze this build's callbacks; changes become visible on the next update.
    std::pmr::vector<std::function<void(Schedule&)>> callbacks;
    for (const auto id : m_RegistrationOrder) {
        const auto& entry = m_Systems.at(id);
        if (entry.enabled && entry.callbacks.configure_schedule) callbacks.emplace_back(entry.callbacks.configure_schedule);
    }
    for (const auto& callback : callbacks) callback(schedule);
}

void SystemManager::Shutdown() {
    if (m_ShuttingDown) return;
    m_ShuttingDown = true;
    while (!m_RegistrationOrder.empty()) {
        UnregisterOne(m_RegistrationOrder.back());
    }
}

}  // namespace hitagi::ecs
