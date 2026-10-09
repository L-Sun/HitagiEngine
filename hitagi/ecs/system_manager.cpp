export module ecs:system_manager;
import std;
import utils;

import :schedule;

namespace hitagi::ecs::detail {
struct SystemCallbacks {
    std::function<void()>          on_create;
    std::function<void()>          on_enable;
    std::function<void(Schedule&)> on_update;
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
    void Register(utils::TypeID id, detail::SystemCallbacks callbacks);
    void Update(Schedule& schedule);
    void Shutdown();
    void EnableOne(utils::TypeID id);
    void DisableOne(utils::TypeID id);
    void UnregisterOne(utils::TypeID id);

    struct Entry {
        detail::SystemCallbacks callbacks;
        bool                    enabled = false;
    };
    std::pmr::unordered_map<utils::TypeID, Entry> m_Systems;
    std::uint64_t                                 m_Revision     = 0;
    bool                                          m_ShuttingDown = false;
};

void SystemManager::Register(utils::TypeID id, detail::SystemCallbacks callbacks) {
    if (m_ShuttingDown) return;
    const auto [entry, inserted] = m_Systems.try_emplace(id, std::move(callbacks));
    if (inserted) {
        // Copy before invoking: a callback may register or unregister systems.
        const auto on_create = entry->second.callbacks.on_create;
        if (on_create) on_create();
    }
    EnableOne(id);
}

void SystemManager::EnableOne(utils::TypeID id) {
    const auto entry = m_Systems.find(id);
    if (entry == m_Systems.end() || entry->second.enabled) return;
    entry->second.enabled = true;
    ++m_Revision;
    const auto callback = entry->second.callbacks.on_enable;
    if (callback) callback();
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
    ++m_Revision;
    if (removed.enabled && removed.callbacks.on_disable) removed.callbacks.on_disable();
    if (removed.callbacks.on_destroy) removed.callbacks.on_destroy();
}

void SystemManager::Update(Schedule& schedule) {
    // Freeze this build's callbacks; changes become visible on the next update.
    std::pmr::vector<std::function<void(Schedule&)>> callbacks;
    for (const auto& [id, entry] : m_Systems) {
        if (entry.enabled && entry.callbacks.on_update) callbacks.emplace_back(entry.callbacks.on_update);
    }
    for (const auto& callback : callbacks) callback(schedule);
}

void SystemManager::Shutdown() {
    if (m_ShuttingDown) return;
    m_ShuttingDown = true;
    std::pmr::vector<utils::TypeID> ids;
    for (const auto& [id, entry] : m_Systems) ids.emplace_back(id);
    for (const auto id : ids) UnregisterOne(id);
}

}  // namespace hitagi::ecs
