#pragma once
#include <hitagi/ecs/entity.hpp>
#include <hitagi/ecs/component.hpp>
#include <hitagi/utils/types.hpp>
#include <hitagi/utils/hash.hpp>
#include <hitagi/utils/soa.hpp>

#include <limits>
#include <optional>
#include <span>
#include <unordered_map>
#include <algorithm>

namespace hitagi::ecs::detail {

using ArchetypeID = std::size_t;

template <utils::no_cvref... Components>
auto get_archetype_id(const DynamicComponents& dynamic_components = {}) -> ArchetypeID
    requires utils::unique_types<Components...>
{
    ArchetypeID archetype_id = 0;
    if constexpr (sizeof...(Components) != 0) {
        auto type_index_array = std::array{utils::TypeID::Create<Components>().GetValue()...};
        std::sort(type_index_array.begin(), type_index_array.end());
        archetype_id = utils::combine_hash(type_index_array);
    }

    {
        std::pmr::vector<std::size_t> dynamic_component_type_id;
        dynamic_component_type_id.reserve(dynamic_components.size());
        std::transform(
            dynamic_components.begin(),
            dynamic_components.end(),
            std::back_inserter(dynamic_component_type_id),
            [](const DynamicComponent& info) { return utils::TypeID{info.name}.GetValue(); });

        archetype_id = utils::combine_hash(dynamic_component_type_id, archetype_id);
    }
    return archetype_id;
}

class IArchetype {
public:
    IArchetype(ArchetypeID id) noexcept : m_ID(id){};
    virtual ~IArchetype() = default;

    inline auto ID() const noexcept { return m_ID; }

    inline auto NumEntities() const noexcept { return m_EntityMap.size(); };

    template <Component T>
    inline bool HasComponent() const noexcept { return m_ComponentIndexMap.contains(utils::TypeID::Create<T>()); }
    inline bool HasComponent(std::string_view name) const noexcept { return m_ComponentIndexMap.contains(utils::TypeID{name}); }
    inline bool HasComponent(utils::TypeID type) const noexcept { return m_ComponentIndexMap.contains(type); }

    template <typename Component>
    auto GetComponentArray() -> std::span<std::remove_cvref_t<Component>>;
    auto GetDynamicCompoentArray(std::string_view name) -> std::pair<void*, std::size_t>;

    virtual void CreateEntities(const std::pmr::vector<Entity>& entities) = 0;
    virtual void DeleteEntity(Entity entity)                              = 0;

    template <typename Component>
    auto GetComponent(Entity entity) -> utils::optional_ref<Component>;
    auto GetDynamicComponent(Entity entity, std::string_view name) -> void*;

protected:
    template <typename Component>
    auto GetComponentIndex() const noexcept -> std::size_t;
    auto GetEntityIndex(Entity entity) const -> std::size_t;

    virtual auto GetComponentRawData(std::size_t component_index, std::size_t entity_index = 0) -> void* = 0;

    ArchetypeID m_ID;

    std::pmr::unordered_map<Entity, std::size_t>        m_EntityMap;
    std::pmr::unordered_map<utils::TypeID, std::size_t> m_ComponentIndexMap;
};

template <utils::no_cvref... Components>
    requires utils::unique_types<Components...>
class Archetype : public IArchetype {
public:
    Archetype(const DynamicComponents& dynamic_components = {});
    Archetype(const Archetype&)            = delete;
    Archetype& operator=(const Archetype&) = delete;

    void CreateEntities(const std::pmr::vector<Entity>& entities) final;
    void DeleteEntity(Entity entity) final;

private:
    auto GetComponentRawData(const std::size_t component_index, std::size_t entity_index = 0) -> void* final;

    using DynamicComponentArray = std::pmr::vector<std::byte>;

    utils::SoA<Entity, Components...>                                    m_StaticComponents;
    std::pmr::vector<std::pair<DynamicComponentArray, DynamicComponent>> m_DynamicComponents;
};

template <typename Component>
auto IArchetype::GetComponentArray() -> std::span<std::remove_cvref_t<Component>> {
    using _Component = std::remove_cvref_t<Component>;

    auto pointer = GetComponentRawData(GetComponentIndex<Component>());

    if (pointer == nullptr) {
        return {(_Component*)(nullptr), 0};
    }

    return {reinterpret_cast<_Component*>(pointer), NumEntities()};
}

template <typename Component>
auto IArchetype::GetComponent(Entity entity) -> utils::optional_ref<Component> {
    const std::size_t index = GetEntityIndex(entity);
    if (index == NumEntities()) return std::nullopt;

    auto result = GetComponentArray<Component>();

    if (result.empty())
        return std::nullopt;
    else
        return result.front();
}

template <typename Component>
auto IArchetype::GetComponentIndex() const noexcept -> std::size_t {
    const auto comonent_id = utils::TypeID::Create<Component>();
    if (m_ComponentIndexMap.contains(comonent_id))
        return m_ComponentIndexMap.at(comonent_id);
    else
        return std::numeric_limits<std::size_t>::max();
}

template <utils::no_cvref... Components>
    requires utils::unique_types<Components...>
Archetype<Components...>::Archetype(const DynamicComponents& dynamic_components)
    : IArchetype(get_archetype_id<Components...>(dynamic_components)) {
    m_ComponentIndexMap.emplace(utils::TypeID::Create<Entity>(), m_ComponentIndexMap.size());

    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (m_ComponentIndexMap.emplace(utils::TypeID::Create<Components>(), m_ComponentIndexMap.size()), ...);
    }(std::index_sequence_for<Components...>{});

    for (const auto& dynamic_components_info : dynamic_components) {
        utils::TypeID dynamic_component_id{dynamic_components_info.name};
        m_DynamicComponents.emplace_back(DynamicComponentArray{}, dynamic_components_info);
        m_ComponentIndexMap.emplace(dynamic_component_id, m_ComponentIndexMap.size());
    }
}

template <utils::no_cvref... Components>
    requires utils::unique_types<Components...>
auto Archetype<Components...>::GetComponentRawData(std::size_t component_index, std::size_t entity_index) -> void* {
    if (component_index == std::numeric_limits<std::size_t>::max())
        return nullptr;

    if (component_index < m_StaticComponents.NumTypes) {
        auto result = [&]<std::size_t... I>(std::index_sequence<I...>) {
            void* result = nullptr;
            ((I == component_index ? (result = &m_StaticComponents.template elements<I>().at(entity_index)) : nullptr), ...);
            return result;
        }(std::index_sequence_for<Entity, Components...>{});

        return result;
    } else {
        auto& [dynamic_component_array, dynamic_component_info] = m_DynamicComponents.at(component_index - m_StaticComponents.NumTypes);
        return dynamic_component_array.data() + entity_index * dynamic_component_info.size;
    }
}

template <utils::no_cvref... Components>
    requires utils::unique_types<Components...>
void Archetype<Components...>::CreateEntities(const std::pmr::vector<Entity>& entities) {
    m_StaticComponents.reserve(m_StaticComponents.size() + entities.size());
    for (const auto& entity : entities) {
        m_EntityMap.emplace(entity, m_StaticComponents.size());
        m_StaticComponents.emplace_back(Entity{entity.id}, Components{}...);
    }
    for (auto& [dynamic_component_array, dynamic_component_info] : m_DynamicComponents) {
        dynamic_component_array.resize(NumEntities() * dynamic_component_info.size);
        if (dynamic_component_info.constructor) {
            std::byte* p_dynamic_component = dynamic_component_array.data() + (NumEntities() - 1) * dynamic_component_info.size;
            dynamic_component_info.constructor(p_dynamic_component);
        }
    }
}

template <utils::no_cvref... Components>
    requires utils::unique_types<Components...>
void Archetype<Components...>::DeleteEntity(Entity entity) {
    std::size_t index = m_EntityMap.at(entity);

    // delete static components
    {
        // This is a reference
        auto a_ref = m_StaticComponents.at(index);
        auto b_ref = m_StaticComponents.back();

        m_EntityMap.at(std::get<0>(b_ref)) = index;

        std::swap(a_ref, b_ref);
        m_StaticComponents.pop_back();
    }
    // delete dynamic components
    for (auto& [dynamic_component_array, dynamic_component_info] : m_DynamicComponents) {
        std::swap_ranges(
            // b_ref
            std::prev(dynamic_component_array.end(), dynamic_component_info.size),
            dynamic_component_array.end(),
            // a_ref
            std::next(dynamic_component_array.begin(), index * dynamic_component_info.size));

        if (dynamic_component_info.deconstructor) {
            std::byte* p_dynamic_component = dynamic_component_array.data() + (NumEntities() - 1) * dynamic_component_info.size;
            dynamic_component_info.deconstructor(p_dynamic_component);
        }

        dynamic_component_array.erase(std::prev(dynamic_component_array.end(), dynamic_component_info.size), dynamic_component_array.end());
    }
    m_EntityMap.erase(entity);
}

}  // namespace hitagi::ecs::detail