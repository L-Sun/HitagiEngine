#include <hitagi/ecs/system_manager.hpp>
#include <hitagi/ecs/schedule.hpp>

namespace hitagi::ecs {

SystemManager::~SystemManager() {
    const auto enabled_systems = m_EnabledSystems;
    for (auto&& id : enabled_systems) {
        UnRegisterOne(id);
    }

    const auto disabled_systems = m_DisabledSystems;
    for (auto&& id : disabled_systems) {
        UnRegisterOne(id);
    }
}

void SystemManager::Update(Schedule& schedule) {
    for (auto&& id : m_EnabledSystems) {
        UpdateOne(id, schedule);
    }
}

void SystemManager::EnableOne(utils::TypeID id) {
    if (m_EnabledSystems.contains(id))
        return;

    m_DisabledSystems.erase(id);
    m_EnabledSystems.emplace(id);

    if (m_OnEnableFns.contains(id))
        m_OnEnableFns.at(id)(m_World);
}

void SystemManager::UpdateOne(utils::TypeID id, Schedule& schedule) {
    if (m_DisabledSystems.contains(id))
        return;

    if (m_OnUpdateFns.contains(id))
        m_OnUpdateFns.at(id)(schedule);
}

void SystemManager::DisableOne(utils::TypeID id) {
    if (m_DisabledSystems.contains(id)) return;

    m_EnabledSystems.erase(id);
    m_DisabledSystems.emplace(id);

    if (m_OnDisableFns.contains(id))
        m_OnDisableFns.at(id)(m_World);
}

void SystemManager::UnRegisterOne(utils::TypeID id) {
    DisableOne(id);
    m_DisabledSystems.erase(id);

    if (m_OnDestroyFns.contains(id))
        m_OnDestroyFns.at(id)(m_World);

    m_OnCreateFns.erase(id);
    m_OnEnableFns.erase(id);
    m_OnUpdateFns.erase(id);
    m_OnDisableFns.erase(id);
    m_OnDestroyFns.erase(id);
}

}  // namespace hitagi::ecs