#pragma once
#include <hitagi/utils/types.hpp>

#include <unordered_set>

namespace hitagi::ecs {
class World;
class Schedule;

class SystemManager {
public:
    SystemManager(World& world) : m_World(world) {}
    ~SystemManager();

    template <typename... Systems>
    void Register();

    template <typename... Systems>
    void Enable();

    template <typename... Systems>
    void Disable();

    template <typename... Systems>
    void Unregister();

private:
    friend World;

    void Update(Schedule& schedule);

    template <typename System>
    void RegisterOne();

    void EnableOne(utils::TypeID id);
    void UpdateOne(utils::TypeID id, Schedule& schedule);
    void DisableOne(utils::TypeID id);
    void UnRegisterOne(utils::TypeID id);

    World& m_World;

    std::pmr::unordered_set<utils::TypeID> m_EnabledSystems;
    std::pmr::unordered_set<utils::TypeID> m_DisabledSystems;

    std::unordered_map<utils::TypeID, std::function<void(World&)>>    m_OnCreateFns;
    std::unordered_map<utils::TypeID, std::function<void(World&)>>    m_OnEnableFns;
    std::unordered_map<utils::TypeID, std::function<void(Schedule&)>> m_OnUpdateFns;
    std::unordered_map<utils::TypeID, std::function<void(World&)>>    m_OnDisableFns;
    std::unordered_map<utils::TypeID, std::function<void(World&)>>    m_OnDestroyFns;
};

namespace detail {
template <typename System>
concept HasOnCreateFn = requires(World& world) {
    { System::OnCreate(world) } -> std::same_as<void>;
};

template <typename System>
concept HasOnEnableFn = requires(World& world) {
    { System::OnEnable(world) } -> std::same_as<void>;
};

template <typename System>
concept HasOnUpdateFn = requires(Schedule& schedule) {
    { System::OnUpdate(schedule) } -> std::same_as<void>;
};

template <typename System>
concept HasOnDisableFn = requires(World& world) {
    { System::OnDisable(world) } -> std::same_as<void>;
};

template <typename System>
concept HasOnDestroyFn = requires(World& world) {
    { System::OnDestroy(world) } -> std::same_as<void>;
};

}  // namespace detail

template <typename System>
void SystemManager::RegisterOne() {
    const auto id = utils::TypeID::Create<System>();
    if constexpr (detail::HasOnCreateFn<System>) {
        const bool success = m_OnCreateFns.emplace(id, &System::OnCreate).second;
        if (success) m_OnCreateFns.at(id)(m_World);
    }
    if constexpr (detail::HasOnEnableFn<System>) {
        m_OnEnableFns.emplace(id, &System::OnEnable);
    }
    if constexpr (detail::HasOnUpdateFn<System>) {
        m_OnUpdateFns.emplace(id, &System::OnUpdate);
    }
    if constexpr (detail::HasOnDisableFn<System>) {
        m_OnDisableFns.emplace(id, &System::OnDisable);
    }
    if constexpr (detail::HasOnDestroyFn<System>) {
        m_OnDestroyFns.emplace(id, &System::OnDestroy);
    }

    EnableOne(id);
}

template <typename... Systems>
void SystemManager::Register() {
    (RegisterOne<Systems>(), ...);
}

template <typename... Systems>
void SystemManager::Unregister() {
    (UnRegisterOne(utils::TypeID::Create<Systems>()), ...);
}

}  // namespace hitagi::ecs
