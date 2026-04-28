module;
#include <taskflow/taskflow.hpp>
#include <spdlog/logger.h>

export module ecs;
import std;
import utils;
import core;

export namespace hitagi::ecs {
using entity_id_t    = std::uint64_t;
using archetype_id_t = std::uint64_t;

class Archetype;
class World;
class Schedule;
class Entity;
class EntityManager;

template <typename T>
concept Component = std::is_class_v<T> && utils::no_cvref<T> && std::copy_constructible<T>;

struct ComponentInfo {
    std::pmr::string name;
    utils::TypeID    type_id;
    std::size_t      size;

    std::function<void(std::byte*)>                   default_constructor;
    std::function<void(std::byte*, const std::byte*)> copy_constructor;
    std::function<void(std::byte*, std::byte*)>       move_constructor;
    std::function<void(std::byte*)>                   destructor;

    constexpr auto operator<=>(const ComponentInfo& rhs) const noexcept {
        return std::tie(size, type_id) <=> std::tie(rhs.size, rhs.type_id);
    }
    constexpr auto operator==(const ComponentInfo& rhs) const noexcept {
        return type_id == rhs.type_id;
    }
    constexpr auto operator!=(const ComponentInfo& rhs) const noexcept {
        return type_id != rhs.type_id;
    }
};

using DynamicComponentSet  = std::pmr::unordered_set<std::pmr::string>;
using DynamicComponentList = std::pmr::vector<std::pmr::string>;

}  // namespace hitagi::ecs

namespace hitagi::ecs::detail {

using ComponentInfoSet = std::pmr::set<ComponentInfo>;

template <Component T>
constexpr auto create_static_component_info() noexcept {
    return ComponentInfo{
        .name                = typeid(T).name(),
        .type_id             = utils::TypeID::Create<T>(),
        .size                = sizeof(T),
        .default_constructor = [](std::byte* ptr) {
                if constexpr(std::is_default_constructible_v<T>) {
                    std::construct_at(reinterpret_cast<T*>(ptr));
                } },
        .copy_constructor    = [](std::byte* ptr, const std::byte* other) { std::construct_at(reinterpret_cast<T*>(ptr), *reinterpret_cast<const T*>(other)); },
        .move_constructor    = [](std::byte* ptr, std::byte* other) { std::construct_at(reinterpret_cast<T*>(ptr), std::move(*reinterpret_cast<T*>(other))); },
        .destructor          = [](std::byte* ptr) { std::destroy_at(reinterpret_cast<T*>(ptr)); },
    };
}

template <Component... Components>
    requires utils::unique_types<Components...>
auto create_component_info_set(const ComponentInfoSet& dynamic_components = {}) noexcept {
    ComponentInfoSet result = {create_static_component_info<Components>()...};
    for (auto dynamic_component : dynamic_components) {
        dynamic_component.type_id = utils::TypeID(dynamic_component.name);
        result.emplace(dynamic_component);
    }
    return result;
}

using ComponentIdSet  = std::pmr::unordered_set<utils::TypeID>;
using ComponentIdList = std::pmr::vector<utils::TypeID>;

template <Component... Components>
auto create_component_id_set(const DynamicComponentSet& dynamic_components = {}) noexcept {
    ComponentIdSet result = {utils::TypeID::Create<Components>()...};
    for (const auto& dynamic_component : dynamic_components) {
        result.emplace(dynamic_component);
    }
    return result;
}

template <Component... Components>
auto create_component_id_list(const DynamicComponentList& dynamic_components = {}) noexcept {
    ComponentIdList result = {utils::TypeID::Create<Components>()...};
    for (const auto& dynamic_component : dynamic_components) {
        result.emplace_back(dynamic_component);
    }
    return result;
}

template <typename T>
concept ComponentValueType = Component<T>;

template <typename T>
concept ComponentConstValueType = Component<std::remove_cv_t<T>> && std::is_const_v<T>;

template <typename T>
concept ComponentReferenceType = Component<std::decay_t<T>> && utils::is_no_const_reference_v<T>;

template <typename T>
concept ComponentConstReferenceType = Component<std::remove_cvref_t<T>> && utils::is_const_reference_v<T>;

template <typename T>
concept DynamicComponentPointerType = std::same_as<T, std::byte*>;

template <typename T>
concept DynamicComponentConstPointerType = std::same_as<T, const std::byte*>;

}  // namespace hitagi::ecs::detail

template <>
struct std::hash<hitagi::ecs::ComponentInfo> {
    std::size_t operator()(const hitagi::ecs::ComponentInfo& info) const noexcept {
        return info.type_id.GetValue();
    }
};

export namespace hitagi::ecs {

class ComponentChecker {
public:
    template <Component T>
    inline bool Exists() const noexcept;
    inline bool Exists(std::string_view dynamic_component) const noexcept;

private:
    friend class EntityManager;
    ComponentChecker(Archetype* archetype) : m_Archetype(archetype) {}

    bool Exists(utils::TypeID component) const noexcept;

    Archetype* m_Archetype;
};

using Filter = std::function<bool(const ComponentChecker&)>;

namespace filter {
template <Component... Components>
Filter All(const DynamicComponentSet& dynamic_components = {}) noexcept {
    return [=](const ComponentChecker& checker) {
        bool result = (checker.Exists<Components>() && ...);
        for (const auto& dynamic_component : dynamic_components) {
            result &= checker.Exists(dynamic_component);
        }
        return result;
    };
}

template <Component... Components>
Filter Any(const DynamicComponentSet& dynamic_components = {}) noexcept {
    return [=](const ComponentChecker& checker) {
        bool result = (checker.Exists<Components>() || ...);
        for (const auto& dynamic_component : dynamic_components) {
            result |= checker.Exists(dynamic_component);
        }
        return result;
    };
}

template <Component... Components>
Filter None(const DynamicComponentSet& dynamic_components = {}) noexcept {
    return [=](const ComponentChecker& checker) {
        bool result = !(checker.Exists<Components>() || ...);
        for (const auto& dynamic_component : dynamic_components) {
            result &= !checker.Exists(dynamic_component);
        }
        return result;
    };
}

}  // namespace filter

template <Component T>
inline bool ComponentChecker::Exists() const noexcept {
    return Exists(utils::TypeID::Create<T>());
}
inline bool ComponentChecker::Exists(std::string_view dynamic_component) const noexcept {
    return Exists(utils::TypeID(dynamic_component));
}

class Archetype {
public:
    Archetype(detail::ComponentInfoSet component_infos);
    ~Archetype();

    const auto& GetComponentInfoSet() const noexcept { return m_ComponentInfoSet; }

    void AllocateFor(entity_id_t entity) noexcept;
    void DeallocateFor(entity_id_t entity) noexcept;

    template <Component T, typename... Args>
    auto ConstructComponent(entity_id_t entity, Args&&... args) -> T&;

    template <Component T>
    void DestructComponent(entity_id_t entity);

    template <Component T>
    auto GetComponent(entity_id_t entity) noexcept -> T&;

    bool HasComponent(utils::TypeID component) const noexcept;

    void DefaultConstructComponent(utils::TypeID component_id, entity_id_t entity);
    void CopyConstructComponent(utils::TypeID component_id, entity_id_t entity, const std::byte* src);
    void MoveConstructComponent(utils::TypeID component_id, entity_id_t entity, std::byte* src);
    void DestructComponent(utils::TypeID component_id, entity_id_t entity) noexcept;
    void DestructAllComponents(entity_id_t entity) noexcept;

    auto GetComponentData(utils::TypeID component_id, entity_id_t entity) noexcept -> std::byte*;
    auto GetAllComponentData(entity_id_t entity) noexcept -> std::unordered_map<utils::TypeID, std::byte*>;

    auto GetComponentBuffers(utils::TypeID component_id) const noexcept -> std::pmr::vector<std::pair<std::byte*, std::size_t>>;

private:
    constexpr static auto sm_chunk_size = 2_kB;
    constexpr static auto sm_align_size = 64;

    struct ChunkInfo {
        std::size_t                                         num_entities_per_chunk;
        std::pmr::unordered_map<utils::TypeID, std::size_t> component_offsets;
    };

    struct Chunk {
        Chunk();
        Chunk(const Chunk&)            = delete;
        Chunk(Chunk&&)                 = default;
        Chunk& operator=(const Chunk&) = delete;
        Chunk& operator=(Chunk&&)      = default;

        std::size_t  num_entity_in_chunk = 0;
        core::Buffer data;
    };

    auto GetComponentInfo(utils::TypeID component_id) const noexcept -> const ComponentInfo&;
    auto GetComponentOffset(utils::TypeID component_id) const noexcept -> std::size_t;
    auto GetOrCreateChunk() noexcept -> Chunk&;
    auto GetLastEntity() const -> entity_id_t;

    detail::ComponentInfoSet m_ComponentInfoSet;
    ChunkInfo                m_ChunkInfo;
    std::pmr::vector<Chunk>  m_Chunks;

    std::pmr::unordered_map<entity_id_t, std::pair<std::size_t, std::size_t>> m_EntityMap;
};

template <Component T, typename... Args>
auto Archetype::ConstructComponent(entity_id_t entity, Args&&... args) -> T& {
    return *std::construct_at<T>(&GetComponent<T>(entity), std::forward<Args>(args)...);
}

template <Component T>
void Archetype::DestructComponent(entity_id_t entity) {
    std::destroy_at<T>(&GetComponent<T>(entity));
}

template <Component T>
auto Archetype::GetComponent(entity_id_t entity) noexcept -> T& {
    return *reinterpret_cast<T*>(GetComponentData(utils::TypeID::Create<T>(), entity));
}

class EntityManager {
public:
    ~EntityManager();

    void RegisterDynamicComponent(ComponentInfo dynamic_component);
    auto GetDynamicComponentInfo(std::string_view dynamic_component) const -> const ComponentInfo&;

    bool Has(Entity entity) const noexcept;

    [[nodiscard]] auto Create() noexcept -> Entity;
    void               Destroy(Entity& entity);

    template <Component... Components>
        requires((std::default_initializable<Components> && utils::not_same_as<Components, Entity>) && ...)
    [[nodiscard]] auto CreateMany(std::size_t num, const std::pmr::set<std::string_view>& dynamic_components = {}) -> std::pmr::vector<Entity>;

    auto NumEntities() const noexcept { return m_EntityMaps.size(); }

private:
    friend World;
    friend Schedule;
    friend Entity;

    EntityManager(World& world);

    auto CreateMany(std::size_t num, const detail::ComponentInfoSet& component_infos) noexcept -> std::pmr::vector<Entity>;

    template <Component T>
    bool HasComponent(entity_id_t entity) const noexcept;
    bool HasDynamicComponent(entity_id_t entity, std::string_view dynamic_component) const;

    template <Component T, typename... Args>
        requires utils::not_same_as<T, Entity>
    auto EmplaceComponent(entity_id_t entity, Args&&... args) noexcept -> T&;

    auto AddDynamicComponent(entity_id_t entity, std::string_view dynamic_component) -> std::byte*;

    template <Component T>
        requires utils::not_same_as<T, Entity>
    void RemoveComponent(entity_id_t entity) noexcept;
    void RemoveDynamicComponent(entity_id_t entity, std::string_view dynamic_component);

    template <Component T>
    auto GetComponent(entity_id_t entity) const -> T&;
    auto GetDynamicComponent(entity_id_t entity, std::string_view dynamic_component) const -> std::byte*;

    template <Component T>
    void UpdateComponentInfo() noexcept;

    template <Component T>
    auto GetComponentInfo() const noexcept -> const ComponentInfo&;
    auto GetComponentInfo(utils::TypeID component_id) const noexcept -> const ComponentInfo&;

    auto GetOrCreateArchetype(const detail::ComponentInfoSet& component_infos) noexcept -> Archetype&;

    struct ComponentData {
        std::byte*  data;
        std::size_t size;
        std::size_t num_entities;

        auto operator[](std::size_t index) const noexcept -> std::byte* { return data + index * size; }
    };
    auto GetComponentsBuffers(const detail::ComponentIdList& components, Filter filter) const noexcept
        -> std::pmr::vector<std::pmr::vector<ComponentData>>;

    World& m_World;

    std::size_t m_Counter = 0;

    std::pmr::unordered_map<archetype_id_t, std::unique_ptr<Archetype>> m_Archetypes;
    std::pmr::unordered_map<entity_id_t, Archetype*>                    m_EntityMaps;
    std::pmr::unordered_map<utils::TypeID, ComponentInfo>               m_ComponentMap;
};

// EntityManager template implementations

template <Component... Components>
    requires((std::default_initializable<Components> && utils::not_same_as<Components, Entity>) && ...)
[[nodiscard]] auto EntityManager::CreateMany(std::size_t num, const std::pmr::set<std::string_view>& dynamic_components) -> std::pmr::vector<Entity> {
    (UpdateComponentInfo<Components>(), ...);

    auto component_infos = detail::create_component_info_set<Entity, Components...>();
    for (auto dynamic_component : dynamic_components) {
        component_infos.emplace(GetDynamicComponentInfo(dynamic_component));
    }

    return CreateMany(num, component_infos);
}

template <Component T>
bool EntityManager::HasComponent(entity_id_t entity) const noexcept {
    const auto& component_infos = m_EntityMaps.at(entity)->GetComponentInfoSet();
    return std::find_if(component_infos.begin(), component_infos.end(), [](const auto& info) { return info.type_id == utils::TypeID::Create<T>(); }) != component_infos.end();
}

template <Component T, typename... Args>
    requires utils::not_same_as<T, Entity>
auto EntityManager::EmplaceComponent(entity_id_t entity, Args&&... args) noexcept -> T& {
    if (HasComponent<T>(entity)) return GetComponent<T>(entity);

    UpdateComponentInfo<T>();
    const auto new_component_id = utils::TypeID::Create<T>();

    auto& old_archetype   = *m_EntityMaps.at(entity);
    auto  component_infos = old_archetype.GetComponentInfoSet();

    component_infos.emplace(detail::create_static_component_info<T>());
    Archetype& new_archetype = GetOrCreateArchetype(component_infos);

    new_archetype.AllocateFor(entity);

    for (const auto& component_info : component_infos) {
        const auto component_id = component_info.type_id;
        if (new_component_id == component_id) {
            new_archetype.ConstructComponent<T>(entity, std::forward<Args>(args)...);
        } else {
            new_archetype.MoveConstructComponent(component_id, entity, old_archetype.GetComponentData(component_id, entity));
            old_archetype.DestructComponent(component_id, entity);
        }
    }

    old_archetype.DeallocateFor(entity);

    m_EntityMaps[entity] = &new_archetype;

    return new_archetype.GetComponent<T>(entity);
}

template <Component T>
    requires utils::not_same_as<T, Entity>
void EntityManager::RemoveComponent(entity_id_t entity) noexcept {
    if (!HasComponent<T>(entity)) return;

    auto& old_archetype   = *m_EntityMaps.at(entity);
    auto  component_infos = old_archetype.GetComponentInfoSet();

    const auto removed_component_id = utils::TypeID::Create<T>();
    std::erase_if(component_infos, [=](const auto& info) { return info.type_id == removed_component_id; });
    Archetype& new_archetype = GetOrCreateArchetype(component_infos);

    new_archetype.AllocateFor(entity);

    for (const auto& component_info : component_infos) {
        const auto component_id = component_info.type_id;
        new_archetype.MoveConstructComponent(component_id, entity, old_archetype.GetComponentData(component_id, entity));
        old_archetype.DestructComponent(component_id, entity);
    }
    old_archetype.DestructComponent<T>(entity);

    old_archetype.DeallocateFor(entity);

    m_EntityMaps[entity] = &new_archetype;
}

template <Component T>
auto EntityManager::GetComponent(entity_id_t entity) const -> T& {
    if (!HasComponent<T>(entity)) {
        const auto error_message = std::format("Entity({}) does not have the component({})", entity, detail::create_static_component_info<T>().name);
        throw std::invalid_argument(error_message);
    }

    return m_EntityMaps.at(entity)->GetComponent<T>(entity);
}

template <Component T>
void EntityManager::UpdateComponentInfo() noexcept {
    m_ComponentMap.emplace(utils::TypeID::Create<T>(), detail::create_static_component_info<T>());
}

template <Component T>
auto EntityManager::GetComponentInfo() const noexcept -> const ComponentInfo& {
    return GetComponentInfo(utils::TypeID::Create<T>());
}

class Entity {
public:
    Entity()              = default;
    Entity(const Entity&) = default;

    template <Component T>
    bool Has() const;
    bool Has(std::string_view dynamic_component) const;

    template <Component T>
    auto Get() const -> const T&;
    template <Component T>
        requires utils::not_same_as<T, Entity>
    auto Get() -> T&;
    auto Get(std::string_view dynamic_component) -> std::byte*;
    auto Get(std::string_view dynamic_component) const -> const std::byte*;

    template <Component T, typename... Args>
        requires utils::not_same_as<T, Entity>
    auto Emplace(Args&&... args) -> T&;

    auto Add(std::string_view dynamic_component) -> std::byte*;

    template <Component T>
        requires utils::not_same_as<T, Entity>
    void Remove();
    void Remove(std::string_view dynamic_component);

    auto GetId() const noexcept { return m_Id; }
    bool Valid() const noexcept;

    explicit operator bool() const noexcept { return Valid(); }
    bool     operator!() const noexcept { return !Valid(); }
    bool     operator==(const Entity& rhs) const noexcept { return m_EntityManager == rhs.m_EntityManager && m_Id == rhs.m_Id; }
    bool     operator!=(const Entity& rhs) const noexcept { return !(*this == rhs); }

    friend struct std::formatter<hitagi::ecs::Entity>;
    friend std::hash<Entity>;

private:
    friend EntityManager;

    Entity(EntityManager* manager, entity_id_t id) : m_EntityManager(manager), m_Id(id) {}

    void CheckValidation() const;

    EntityManager* m_EntityManager = nullptr;
    entity_id_t    m_Id            = std::numeric_limits<entity_id_t>::max();
};

// Entity template implementations

template <Component T>
bool Entity::Has() const {
    CheckValidation();
    return m_EntityManager->HasComponent<T>(m_Id);
}

template <Component T, typename... Args>
    requires utils::not_same_as<T, Entity>
auto Entity::Emplace(Args&&... args) -> T& {
    CheckValidation();
    return m_EntityManager->EmplaceComponent<T>(m_Id, std::forward<Args>(args)...);
}

template <Component T>
    requires utils::not_same_as<T, Entity>
void Entity::Remove() {
    CheckValidation();
    m_EntityManager->RemoveComponent<T>(m_Id);
}

template <Component T>
    requires utils::not_same_as<T, Entity>
auto Entity::Get() -> T& {
    CheckValidation();
    return m_EntityManager->GetComponent<T>(m_Id);
}

template <Component T>
auto Entity::Get() const -> const T& {
    CheckValidation();
    return m_EntityManager->GetComponent<T>(m_Id);
}

}  // namespace hitagi::ecs

template <>
struct std::hash<hitagi::ecs::Entity> {
    constexpr std::size_t operator()(const hitagi::ecs::Entity& entity) const {
        return static_cast<std::size_t>(entity.m_Id);
    }
};

template <>
struct std::formatter<hitagi::ecs::Entity> : std::formatter<hitagi::ecs::entity_id_t> {
    auto format(const hitagi::ecs::Entity& entity, std::format_context& ctx) const {
        return std::formatter<hitagi::ecs::entity_id_t>::format(entity.m_Id, ctx);
    }
};

export namespace hitagi::ecs {

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

// SystemManager template implementations

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
void SystemManager::Enable() {
    (EnableOne(utils::TypeID::Create<Systems>()), ...);
}

template <typename... Systems>
void SystemManager::Disable() {
    (DisableOne(utils::TypeID::Create<Systems>()), ...);
}

template <typename... Systems>
void SystemManager::Unregister() {
    (UnRegisterOne(utils::TypeID::Create<Systems>()), ...);
}

class Schedule;

class World {
public:
    World(std::string_view name, core::JobSystem* job_system = core::JobSystem::Get());
    ~World();

    void Update();

    inline auto  GetName() const noexcept -> std::string_view { return m_Name; }
    inline auto& GetEntityManager() noexcept { return m_EntityManager; }
    inline auto& GetSystemManager() noexcept { return m_SystemManager; }
    inline auto& GetEntityManager() const noexcept { return m_EntityManager; }
    inline auto& GetSystemManager() const noexcept { return m_SystemManager; }
    inline auto  GetLogger() noexcept { return m_Logger; }

private:
    friend SystemManager;

    void InvalidateSchedule() noexcept;

    std::pmr::string                m_Name;
    std::shared_ptr<spdlog::logger> m_Logger;

    EntityManager             m_EntityManager;
    std::unique_ptr<Schedule> m_Schedule;
    bool                      m_ScheduleDirty = true;
    core::JobSystem*          m_JobSystem     = nullptr;
    SystemManager             m_SystemManager;
};

class Schedule {
    struct TaskBase {
        TaskBase(std::string_view name, detail::ComponentIdList component_list, Filter filter)
            : name(name), component_list(std::move(component_list)), filter(std::move(filter)) {}

        virtual void Run(World&) = 0;

        std::pmr::string        name;
        detail::ComponentIdList component_list;
        Filter                  filter;
    };

    template <typename Func>
    struct Task : public TaskBase {
        Task(std::string_view name, detail::ComponentIdList component_list, Filter filter, Func&& task)
            : TaskBase(name, std::move(component_list), std::move(filter)), task(std::move(task)) {}

        void Run(World& world) final;
        Func task;
    };

public:
    Schedule(World& world) : world(world) {}

    template <typename Func>
    Schedule& Request(
        std::string_view     name,
        Func&&               task,
        DynamicComponentList dynamic_components = {},
        Filter               filter             = {});

    void SetOrder(std::string_view first_task, std::string_view second_task);

    World& world;

private:
    friend World;

    using ReadBeforeWriteParameters = std::pmr::set<utils::TypeID>;
    using WriteParameters           = std::pmr::set<utils::TypeID>;
    using ReadAfterWriteParameters  = std::pmr::set<utils::TypeID>;
    using ParameterSets             = std::tuple<ReadBeforeWriteParameters, WriteParameters, ReadAfterWriteParameters>;

    template <typename T>
    static void FillParameterSets(ParameterSets& parameter_sets, std::string_view dynamic_component);

    void Request(std::shared_ptr<TaskBase>&& task, const ParameterSets& parameter_sets);

    void Run(core::JobSystem& job_system);
    void BuildTaskflow(core::JobSystem& job_system);

    bool CheckValid(const std::pmr::unordered_map<std::size_t, std::pmr::unordered_set<std::size_t>>& graph);

    std::pmr::vector<std::shared_ptr<TaskBase>>                           m_Tasks;
    std::pmr::unordered_map<std::pmr::string, std::size_t>                m_TaskNameToIndex;
    std::pmr::unordered_map<utils::TypeID, std::pmr::vector<std::size_t>> m_ReadBeforeWriteSet;
    std::pmr::unordered_map<utils::TypeID, std::pmr::vector<std::size_t>> m_WriteSet;
    std::pmr::unordered_map<utils::TypeID, std::pmr::vector<std::size_t>> m_ReadAfterWriteSet;

    std::pmr::unordered_map<std::pmr::string, std::pmr::string> m_CustomOrder;

    tf::Taskflow               m_Taskflow;
    std::pmr::vector<tf::Task> m_TaskflowTasks;
    bool                       m_TaskflowDirty = true;
};

}  // namespace hitagi::ecs

namespace hitagi::ecs {

export template <typename T>
    requires detail::ComponentValueType<T> ||
             detail::ComponentConstValueType<T> ||
             detail::ComponentConstReferenceType<T> ||
             detail::DynamicComponentConstPointerType<T>
struct LastFrame {
    using type = T;

    inline operator T() const noexcept { return value; }

    T value;
};

namespace detail {

template <typename T>
concept HasLastFrameTag = std::same_as<T, LastFrame<typename T::type>>;

template <typename T>
concept ReadBeforWriteParameter = HasLastFrameTag<T> &&
                                  (ComponentValueType<typename T::type> ||
                                   ComponentConstReferenceType<typename T::type> ||
                                   DynamicComponentConstPointerType<typename T::type>);

template <typename T>
concept WriteParameter =
    !HasLastFrameTag<std::decay_t<T>> &&
    !std::same_as<T, Entity&> &&
    (ComponentReferenceType<T> || DynamicComponentPointerType<T>);

template <typename T>
concept ReadAfterWriteParameter =
    !HasLastFrameTag<std::decay_t<T>> && (ComponentValueType<T> ||
                                          ComponentConstValueType<T> ||
                                          ComponentConstReferenceType<T> ||
                                          DynamicComponentConstPointerType<T>);

template <typename T>
concept ValidParameter = ReadBeforWriteParameter<T> || WriteParameter<T> || ReadAfterWriteParameter<T>;

template <typename T>
using remove_last_frame_tag_t = std::conditional_t<HasLastFrameTag<T>, T, utils::delay_type<T>>::type;

template <typename T>
using decay_parameter_t = std::remove_cvref_t<remove_last_frame_tag_t<T>>;

struct Parameter {
    Parameter(std::byte* data) : data(data) {}

    std::byte* data;

    template <typename T>
    inline operator LastFrame<T>() const noexcept {
        if constexpr (DynamicComponentPointerType<T> || DynamicComponentConstPointerType<T>) {
            return {data};
        } else {
            return {*reinterpret_cast<T*>(data)};
        }
    };

    template <typename T>
        requires(!DynamicComponentPointerType<T> && !DynamicComponentConstPointerType<T>)
    inline operator T&() const noexcept {
        return *reinterpret_cast<T*>(data);
    };

    template <typename T>
        requires(DynamicComponentPointerType<T> || DynamicComponentConstPointerType<T>)
    inline operator T() const noexcept {
        return data;
    };
};

}  // namespace detail

template <typename Func>
Schedule& Schedule::Request(std::string_view name, Func&& task, DynamicComponentList dynamic_components, Filter filter) {
    using traits = utils::function_traits<Func>;

    static_assert(traits::args_size > 0, "Task must request at least one component");

    []<std::size_t... I>(std::index_sequence<I...>) {
        static_assert((detail::ValidParameter<typename traits::template arg_t<I>> && ...), "Invalid parameter type");
    }(std::make_index_sequence<traits::args_size>{});

    std::bitset<traits::args_size> dynamic_component_flags;
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        static_assert((!(utils::remove_const_pointer_same<detail::decay_parameter_t<typename traits::template arg_t<I - 1>>, std::byte*> &&
                         Component<detail::decay_parameter_t<typename traits::template arg_t<I>>>) &&
                       ...),
                      "dynamic parameter pointers must be after component types");
        ((dynamic_component_flags[I - 1] = utils::remove_const_pointer_same<detail::decay_parameter_t<typename traits::template arg_t<I - 1>>, std::byte*>), ...);
    }(utils::make_index_sequence_from_to<1, traits::args_size>{});

    constexpr std::size_t first_dynamic_component_index = [&]<std::size_t... I>(std::index_sequence<I...>) {
        return utils::first_index_of_v<std::byte*, utils::remove_const_pointer_t<detail::decay_parameter_t<typename traits::template arg_t<I>>>...>;
    }(std::make_index_sequence<traits::args_size>{});

    ParameterSets parameter_sets;
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (FillParameterSets<typename traits::template arg_t<I>>(
             parameter_sets,
             I < first_dynamic_component_index ? "" : dynamic_components[I - first_dynamic_component_index]),
         ...);
    }(std::make_index_sequence<traits::args_size>{});

    auto component_id_list = [&]<std::size_t... I>(std::index_sequence<I...>) {
        return detail::create_component_id_list<detail::decay_parameter_t<typename traits::template arg_t<I>>...>(dynamic_components);
    }(std::make_index_sequence<first_dynamic_component_index>{});

    Request(std::make_shared<Task<Func>>(name, std::move(component_id_list), std::move(filter), std::forward<Func>(task)), std::move(parameter_sets));

    return *this;
}

template <typename T>
void Schedule::FillParameterSets(ParameterSets& parameter_sets, std::string_view dynamic_component) {
    using ComponentType = detail::decay_parameter_t<T>;

    if constexpr (detail::ReadBeforWriteParameter<T>) {
        if constexpr (utils::remove_const_pointer_same<ComponentType, std::byte*>) {
            std::get<0>(parameter_sets).emplace(utils::TypeID{dynamic_component});
        } else {
            std::get<0>(parameter_sets).emplace(utils::TypeID::Create<ComponentType>());
        }
    } else if constexpr (detail::WriteParameter<T>) {
        if constexpr (utils::remove_const_pointer_same<ComponentType, std::byte*>) {
            std::get<1>(parameter_sets).emplace(utils::TypeID{dynamic_component});
        } else {
            std::get<1>(parameter_sets).emplace(utils::TypeID::Create<ComponentType>());
        }
    } else if constexpr (detail::ReadAfterWriteParameter<T>) {
        if constexpr (utils::remove_const_pointer_same<ComponentType, std::byte*>) {
            std::get<2>(parameter_sets).emplace(utils::TypeID{dynamic_component});
        } else {
            std::get<2>(parameter_sets).emplace(utils::TypeID::Create<ComponentType>());
        }
    }
}

template <typename Func>
void Schedule::Task<Func>::Run(World& world) {
    using traits = utils::function_traits<Func>;

    [&]<std::size_t... I>(std::index_sequence<I...>) {
        const auto components_buffers = world.GetEntityManager().GetComponentsBuffers(component_list, filter);
        if (components_buffers.empty()) return;

        const auto num_buffers = components_buffers.front().size();

        for (std::size_t buffer_index = 0; buffer_index < num_buffers; buffer_index++) {
            const auto num_entities = components_buffers.front()[buffer_index].num_entities;
            for (std::size_t entity_index = 0; entity_index < num_entities; entity_index++) {
                task(detail::Parameter(components_buffers[I][buffer_index][entity_index])...);
            }
        }
    }(std::make_index_sequence<traits::args_size>{});
}

}  // namespace hitagi::ecs
