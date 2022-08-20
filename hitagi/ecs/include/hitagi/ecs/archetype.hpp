#pragma once
#include <hitagi/ecs/entity.hpp>
#include <hitagi/utils/concepts.hpp>
#include <hitagi/utils/hash.hpp>
#include <hitagi/utils/soa.hpp>

#include <optional>
#include <typeindex>
#include <span>
#include <cassert>
#include <unordered_map>
#include <unordered_set>

namespace hitagi::ecs {
using ArchetypeId = std::size_t;

template <typename... Components>
requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...)  //
    constexpr ArchetypeId get_archetype_id() {
    return []<std::size_t... I>(std::index_sequence<I...>) {
        auto type_index_array = std::array{std::type_index(typeid(Components))...};
        std::sort(type_index_array.begin(), type_index_array.end());
        std::size_t seed = 0;
        utils::hash_combine(seed, type_index_array.at(I)...);
        return seed;
    }
    (std::index_sequence_for<Components...>{});
}

class IArchetype {
protected:
    using ComponentInfo = std::type_index;

public:
    IArchetype() = default;

    virtual ArchetypeId Id() const = 0;

    template <typename... Components>
    bool HasComponents() const;

    template <typename Component>
    auto GetComponentArray() -> std::span<std::remove_cvref_t<Component>>;

    virtual std::size_t NumEntities() const = 0;

    virtual void CreateInstances(const std::pmr::vector<Entity>& entities) = 0;
    virtual void DeleteInstance(const Entity& entity)                      = 0;

    template <typename Component>
    std::optional<std::reference_wrapper<Component>> GetComponent(const Entity& entity);

protected:
    template <typename Component>
    bool HasComponent() const;

    template <typename Component>
    std::size_t GetComponentIndex() const;

    virtual std::tuple<void*, std::size_t> GetComponentRawData(const std::size_t index) = 0;

    virtual std::size_t GetEntityIndex(const Entity& entity) const = 0;

    std::pmr::unordered_map<ComponentInfo, std::size_t> m_MetaInfo;
};

template <typename... Components>
requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...) class Archetype : public IArchetype {
public:
    Archetype() = default;

    static std::shared_ptr<IArchetype> Create() {
        auto result = std::make_shared<Archetype>();

        result->m_MetaInfo.emplace(typeid(std::remove_cvref_t<Entity>), result->m_MetaInfo.size());

        [&]<std::size_t... I>(std::index_sequence<I...>) {
            (result->m_MetaInfo.emplace(typeid(std::remove_cvref_t<Components>), result->m_MetaInfo.size()), ...);
        }
        (std::index_sequence_for<Components...>{});

        return result;
    }

    ArchetypeId Id() const final { return get_archetype_id<Components...>(); }

    inline std::size_t NumEntities() const final { return m_Data.size(); }

    void CreateInstances(const std::pmr::vector<Entity>& entities) final;
    void DeleteInstance(const Entity& entity) final;

    inline std::size_t GetEntityIndex(const Entity& entity) const final { return m_IndexMap.at(entity); }

private:
    std::tuple<void*, std::size_t> GetComponentRawData(const std::size_t index) final;

    std::pmr::unordered_map<Entity, std::size_t> m_IndexMap;
    utils::SoA<Entity, Components...>            m_Data;
};

template <typename Component>
bool IArchetype::HasComponent() const {
    return m_MetaInfo.contains(typeid(std::remove_cvref_t<Component>));
}

template <typename... Components>
bool IArchetype::HasComponents() const {
    return (true && ... && HasComponent<Components>());
}

template <typename Component>
auto IArchetype::GetComponentArray() -> std::span<std::remove_cvref_t<Component>> {
    auto [pointer, size] = GetComponentRawData(GetComponentIndex<Component>());

    using _Component = std::remove_cvref_t<Component>;
    return std::span<_Component>{reinterpret_cast<_Component*>(pointer), size};
}

template <typename Component>
auto IArchetype::GetComponent(const Entity& entity) -> std::optional<std::reference_wrapper<Component>> {
    if (!HasComponent<Component>()) return std::nullopt;

    const std::size_t index = GetEntityIndex(entity);
    if (index == NumEntities()) return std::nullopt;

    return GetComponentArray<Component>()[index];
}

template <typename Component>
std::size_t IArchetype::GetComponentIndex() const {
    return m_MetaInfo.at(typeid(std::remove_cvref_t<Component>));
}

template <typename... Components>
requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...)  //
    std::tuple<void*, std::size_t> Archetype<Components...>::GetComponentRawData(const std::size_t index) {
    assert(index < m_MetaInfo.size());

    auto result = [&]<std::size_t... I>(std::index_sequence<I...>) {
        void* result = nullptr;
        ((I == index ? (result = m_Data.template elements<I>().data()) : false), ...);
        return result;
    }
    (std::index_sequence_for<Entity, Components...>{});

    return {result, m_Data.size()};
}

template <typename... Components>
requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...)  //
    void Archetype<Components...>::CreateInstances(const std::pmr::vector<Entity>& entities) {
    m_Data.reserve(m_Data.size() + entities.size());
    for (const auto& entity : entities) {
        m_IndexMap.emplace(entity, m_Data.size());
        m_Data.emplace_back(Entity{entity.id}, Components{}...);
    }
}

template <typename... Components>
requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...)  //
    void Archetype<Components...>::DeleteInstance(const Entity& entity) {
    std::size_t index = m_IndexMap.at(entity);

    // This is a reference
    auto a = m_Data.at(index);
    auto b = m_Data.back();

    m_IndexMap.at(std::get<0>(b)) = index;

    std::swap(a, b);
    m_Data.pop_back();
    m_IndexMap.erase(entity);
}

}  // namespace hitagi::ecs