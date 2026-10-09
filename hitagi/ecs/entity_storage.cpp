export module ecs:entity_storage;
import interop.spdlog;

import std;
import utils;

import :component;
import :archetype;
import :filter;

namespace hitagi::ecs::detail {

// Owns descriptions, archetypes and locations, without knowing Entity or World.
class EntityStorage {
public:
    explicit EntityStorage(std::shared_ptr<spdlog::logger> logger) : m_Logger(std::move(logger)) {}
    EntityStorage(const EntityStorage&)            = delete;
    EntityStorage& operator=(const EntityStorage&) = delete;

    void RegisterDynamicComponent(ComponentInfo component);
    auto GetDynamicComponentInfo(std::string_view name) const -> const ComponentInfo&;
    bool Has(entity_id_t entity) const noexcept { return m_EntityMaps.contains(entity); }
    auto NumEntities() const noexcept { return m_EntityMaps.size(); }
    auto CreateMany(std::size_t num, ComponentIdList components) -> entity_id_t;
    void Destroy(entity_id_t entity);

    template <Component T>
    bool HasComponent(entity_id_t entity) const { return m_EntityMaps.at(entity)->HasComponent(utils::TypeID::Create<T>()); }
    bool HasDynamicComponent(entity_id_t entity, std::string_view name) const {
        return m_EntityMaps.at(entity)->HasComponent(GetDynamicComponentInfo(name).type_id);
    }

    template <Component T, typename... Args>
    auto EmplaceComponent(entity_id_t entity, Args&&... args) -> T& {
        const auto& info = RegisterComponent<T>();
        if (!HasComponent<T>(entity)) {
            Migrate(entity, info.type_id, true, [&](std::byte* data) {
                std::construct_at(reinterpret_cast<T*>(data), std::forward<Args>(args)...);
            });
        }
        return GetComponent<T>(entity);
    }

    auto AddDynamicComponent(entity_id_t entity, std::string_view name) -> std::byte*;

    template <Component T>
    void RemoveComponent(entity_id_t entity) { RemoveComponent(entity, utils::TypeID::Create<T>()); }
    void RemoveDynamicComponent(entity_id_t entity, std::string_view name) {
        RemoveComponent(entity, GetDynamicComponentInfo(name).type_id);
    }

    template <Component T>
    auto GetComponent(entity_id_t entity) const -> T& { return m_EntityMaps.at(entity)->GetComponent<T>(entity); }
    auto GetDynamicComponent(entity_id_t entity, std::string_view name) const -> std::byte* {
        return m_EntityMaps.at(entity)->GetComponentData(GetDynamicComponentInfo(name).type_id, entity);
    }

    template <Component T>
    auto RegisterComponent() -> const ComponentInfo& {
        const auto id = utils::TypeID::Create<T>();
        if (const auto found = m_ComponentMap.find(id); found != m_ComponentMap.end()) return found->second;
        return m_ComponentMap.emplace(id, create_static_component_info<T>()).first->second;
    }

    template <std::size_t N, typename Func>
    void ForEachChunk(std::span<const utils::TypeID> components, const Filter& filter, Func&& visit) const {
        for (const auto& [signature, archetype] : m_Archetypes) {
            if (!std::ranges::all_of(components, [&](utils::TypeID id) { return archetype->HasComponent(id); })) continue;
            // Filters may capture mutable state: evaluate on every traversal.
            if (filter && !filter(ComponentChecker(archetype.get()))) continue;
            archetype->ForEachChunk<N>(components, visit);
        }
    }

private:
    struct SignatureHash {
        auto operator()(const ComponentIdList& signature) const noexcept -> std::size_t {
            std::size_t hash = 0;
            for (const auto id : signature) utils::hash_combine(hash, id);
            return hash;
        }
    };

    auto GetOrCreateArchetype(ComponentIdList components) -> Archetype&;
    void RemoveComponent(entity_id_t entity, utils::TypeID component);

    template <typename Construct>
    void Migrate(entity_id_t entity, utils::TypeID changed, bool adding, Construct&& construct);

    std::shared_ptr<spdlog::logger> m_Logger;
    entity_id_t                     m_Counter = 0;
    // Declared before archetypes: stable descriptions outlive their columns.
    std::pmr::unordered_map<utils::TypeID, ComponentInfo>                               m_ComponentMap;
    std::pmr::unordered_map<ComponentIdList, std::unique_ptr<Archetype>, SignatureHash> m_Archetypes;
    std::pmr::unordered_map<entity_id_t, Archetype*>                                    m_EntityMaps;
};

template <typename Construct>
void EntityStorage::Migrate(entity_id_t entity, utils::TypeID changed, bool adding, Construct&& construct) {
    auto&           source = *m_EntityMaps.at(entity);
    ComponentIdList signature;
    for (const auto& column : source.GetColumns()) {
        if (column.info->type_id != changed) signature.push_back(column.info->type_id);
    }
    if (adding) signature.push_back(changed);
    auto& target = GetOrCreateArchetype(std::move(signature));
    target.AllocateFor(entity);
    // Finish the only potentially throwing user construction before moving old data.
    try {
        if (adding) construct(target.GetComponentData(changed, entity));
    } catch (...) {
        target.DeallocateFor(entity);  // Last row; no components have been constructed.
        throw;
    }
    for (const auto& column : source.GetColumns()) {
        const auto id = column.info->type_id;
        if (id != changed) target.MoveConstructComponent(id, entity, source.GetComponentData(id, entity));
    }
    source.DestructAllComponents(entity);
    source.DeallocateFor(entity);
    m_EntityMaps.at(entity) = &target;
}

void EntityStorage::RegisterDynamicComponent(ComponentInfo component) {
    if (component.size == 0 || !std::has_single_bit(component.alignment) || component.size % component.alignment != 0)
        throw std::invalid_argument("Invalid dynamic component size or alignment");
    component.type_id = utils::TypeID(component.name);
    if (const auto found = m_ComponentMap.find(component.type_id); found != m_ComponentMap.end()) {
        if (found->second.name != component.name || found->second.size != component.size || found->second.alignment != component.alignment)
            throw std::invalid_argument("Conflicting component registration");
        return;
    }
    m_ComponentMap.emplace(component.type_id, std::move(component));
}

auto EntityStorage::GetDynamicComponentInfo(std::string_view name) const -> const ComponentInfo& {
    const auto found = m_ComponentMap.find(utils::TypeID(name));
    if (found == m_ComponentMap.end()) {
        const auto message = std::format("Component {} not registered", name);
        m_Logger->error(message);
        throw std::invalid_argument(message);
    }
    return found->second;
}

auto EntityStorage::GetOrCreateArchetype(ComponentIdList components) -> Archetype& {
    std::ranges::sort(components);
    components.erase(std::unique(components.begin(), components.end()), components.end());
    if (const auto found = m_Archetypes.find(components); found != m_Archetypes.end()) return *found->second;
    std::pmr::vector<const ComponentInfo*> descriptions;
    descriptions.reserve(components.size());
    for (const auto id : components) descriptions.push_back(&m_ComponentMap.at(id));
    auto  archetype = std::make_unique<Archetype>(descriptions);
    auto& result    = *archetype;
    m_Archetypes.emplace(std::move(components), std::move(archetype));
    return result;
}

auto EntityStorage::CreateMany(std::size_t num, ComponentIdList components) -> entity_id_t {
    const entity_id_t first = m_Counter;
    if (num == 0) return first;
    if (num > std::numeric_limits<entity_id_t>::max() - m_Counter) throw std::length_error("Entity IDs exhausted");
    auto& archetype = GetOrCreateArchetype(std::move(components));
    m_Counter += num;  // Never reuse IDs, including failed batches.
    std::size_t created = 0;
    try {
        for (; created < num; ++created) {
            const auto entity = first + created;
            m_EntityMaps.emplace(entity, &archetype);
            try {
                archetype.AllocateFor(entity);
            } catch (...) {
                m_EntityMaps.erase(entity);
                throw;
            }
            std::size_t constructed = 0;
            const auto& columns     = archetype.GetColumns();
            try {
                for (const auto& column : columns) {
                    archetype.DefaultConstructComponent(column.info->type_id, entity);
                    ++constructed;
                }
            } catch (...) {
                while (constructed > 0) archetype.DestructComponent(columns[--constructed].info->type_id, entity);
                archetype.DeallocateFor(entity);
                m_EntityMaps.erase(entity);
                throw;
            }
        }
    } catch (...) {
        while (created > 0) Destroy(first + --created);
        throw;
    }
    return first;
}

void EntityStorage::Destroy(entity_id_t entity) {
    auto& archetype = *m_EntityMaps.at(entity);
    archetype.DestructAllComponents(entity);
    archetype.DeallocateFor(entity);
    m_EntityMaps.erase(entity);
}

auto EntityStorage::AddDynamicComponent(entity_id_t entity, std::string_view name) -> std::byte* {
    const auto& info = GetDynamicComponentInfo(name);
    if (!m_EntityMaps.at(entity)->HasComponent(info.type_id)) {
        Migrate(entity, info.type_id, true, [&](std::byte* data) {
            if (info.default_constructor)
                info.default_constructor(data);
            else
                std::memset(data, 0, info.size);
        });
    }
    return GetDynamicComponent(entity, name);
}

void EntityStorage::RemoveComponent(entity_id_t entity, utils::TypeID component) {
    if (m_EntityMaps.at(entity)->HasComponent(component))
        Migrate(entity, component, false, [](std::byte*) {});
}

}  // namespace hitagi::ecs::detail
