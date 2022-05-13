#pragma once
#include <hitagi/ecs/entity.hpp>
#include <hitagi/utils/concepts.hpp>
#include <hitagi/utils/hash.hpp>
#include <hitagi/utils/soa.hpp>

#include <optional>
#include <typeindex>
#include <span>

namespace hitagi::ecs {
using ArchetypeId = std::size_t;

template <typename... Components>
requires utils::unique_types<Components...>
constexpr ArchetypeId get_archetype_id() {
    std::size_t seed = 0;
    utils::hash_combine(seed, std::type_index(typeid(std::remove_cvref_t<Components>))...);
    return seed;
}

class IArchetype {
protected:
    struct ComponentInfo {
        std::type_index type;
        std::size_t     size;
    };

public:
    using allocator_type = std::pmr::polymorphic_allocator<>;
    IArchetype(allocator_type alloc = {}) : m_MetaInfo(alloc) {}

    virtual ArchetypeId Id() const = 0;

    template <typename... Components>
    bool HasComponents() const;

    template <typename Component>
    auto GetComponentArray() -> std::span<std::remove_cvref_t<Component>>;

    virtual std::size_t NumEntities() const = 0;

    virtual void CreateInstance(const Entity& entity) = 0;
    virtual void DeleteInstance(const Entity& entity) = 0;

    template <typename Component>
    std::optional<std::reference_wrapper<Component>> GetComponent(const Entity& entity);

protected:
    template <typename Component>
    bool HasComponent() const;

    template <typename Component>
    std::size_t GetComponentIndex() const;

    virtual std::tuple<void*, std::size_t> GetComponentRawData(const std::size_t index) = 0;

    virtual std::size_t GetEntityIndex(const Entity& entity) const = 0;

    std::pmr::vector<ComponentInfo> m_MetaInfo;
};

template <typename... Components>
requires utils::unique_types<Components...>
class Archetype : public IArchetype {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;
    Archetype(allocator_type alloc = {}) : m_Data(alloc) {}

    static std::shared_ptr<IArchetype> Create(allocator_type alloc = {}) {
        auto result = std::allocate_shared<Archetype>(alloc);

        // entity info also is a component
        result->m_MetaInfo.emplace_back(
            ComponentInfo{
                .type = std::type_index(typeid(Entity)),
                .size = sizeof(Entity),
            });

        [&]<std::size_t... I>(std::index_sequence<I...>) {
            (result->m_MetaInfo.emplace_back(
                 ComponentInfo{
                     .type = std::type_index(typeid(std::remove_cvref_t<Components>)),
                     .size = sizeof(Components),
                 }),
             ...);
        }
        (std::index_sequence_for<Components...>{});

        return result;
    }

    ArchetypeId Id() const final { return get_archetype_id<Components...>(); }

    std::size_t NumEntities() const final { return m_Data.size(); }

    void CreateInstance(const Entity& entity) final;
    void DeleteInstance(const Entity& entity) final;

private:
    std::tuple<void*, std::size_t> GetComponentRawData(const std::size_t index) final;
    std::size_t                    GetEntityIndex(const Entity& entity) const final;

    utils::SoA<Entity, Components...> m_Data;
};

template <typename Component>
bool IArchetype::HasComponent() const {
    auto iter = std::find_if(
        m_MetaInfo.begin(), m_MetaInfo.end(),
        [&](const ComponentInfo& info) {
            return info.type == std::type_index(typeid(std::remove_cvref_t<Component>));
        });
    return iter != m_MetaInfo.end();
}

template <typename... Components>
bool IArchetype::HasComponents() const {
    return (true && ... && HasComponent<Components>());
}

template <typename Component>
auto IArchetype::GetComponentArray() -> std::span<std::remove_cvref_t<Component>> {
    assert(HasComponent<Component>());
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
    assert(HasComponent<Component>());

    auto iter = std::find_if(
        m_MetaInfo.begin(), m_MetaInfo.end(),
        [&](const ComponentInfo& info) {
            return info.type == std::type_index(typeid(Component));
        });
    return std::distance(m_MetaInfo.begin(), iter);
}

template <typename... Components>
requires utils::unique_types<Components...>
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
requires utils::unique_types<Components...>
void Archetype<Components...>::CreateInstance(const Entity& entity) {
    m_Data.emplace_back(Entity{entity.index}, Components{}...);
}

template <typename... Components>
requires utils::unique_types<Components...>
void Archetype<Components...>::DeleteInstance(const Entity& entity) {
    std::size_t index = GetEntityIndex(entity);

    auto a = m_Data.at(index);
    auto b = m_Data.back();

    std::swap(a, b);
    m_Data.pop_back();
}

template <typename... Components>
requires utils::unique_types<Components...>
    std::size_t Archetype<Components...>::GetEntityIndex(const Entity& entity)
const {
    auto iter = std::find_if(
        m_Data.begin(), m_Data.end(),
        [&](const auto& item) {
            return std::get<0>(item) == entity;
        });

    return std::distance(m_Data.begin(), iter);
}
}  // namespace hitagi::ecs