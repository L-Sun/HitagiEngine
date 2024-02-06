#pragma once
#include <hitagi/ecs/component.hpp>

#include <cstdint>
#include <functional>

namespace hitagi::ecs {

class Archetype;

class Entity {
public:
    Entity()                         = default;
    Entity(const Entity&)            = default;
    Entity(Entity&&)                 = default;
    Entity& operator=(const Entity&) = default;
    Entity& operator=(Entity&&)      = default;

    template <Component T>
    bool HasComponent() const noexcept;
    bool HasComponent(std::string_view dynamic_component) const noexcept;

    template <Component T>
    auto GetComponent() const -> T&;
    auto GetComponent(std::string_view dynamic_component) const -> std::byte*;

    template <Component... Components>
        requires utils::unique_types<Components...> && utils::no_in<Entity, Components...>
    void Attach(const DynamicComponentSet& dynamic_components = {});

    template <Component... Components>
        requires utils::unique_types<Components...> && utils::no_in<Entity, Components...>
    void Detach(const DynamicComponentSet& dynamic_components = {});

    auto     GetId() const noexcept { return m_Id; }
    explicit operator bool() const noexcept;
    bool     operator!() const noexcept { return !static_cast<bool>(*this); }

    bool operator==(const Entity& rhs) const noexcept { return m_EntityManager == rhs.m_EntityManager && m_Id == rhs.m_Id; }
    bool operator!=(const Entity& rhs) const noexcept { return !(*this == rhs); }

    friend auto format_as(const Entity& entity) noexcept { return entity.m_Id; }
    friend std::hash<Entity>;

private:
    friend class EntityManager;

    Entity(EntityManager* manager, std::uint64_t id) : m_EntityManager(manager), m_Id(id) {}

    bool HasComponent(utils::TypeID component_id) const noexcept;
    auto GetComponent(utils::TypeID component_id) const noexcept -> std::byte*;
    void Attach(detail::ComponentInfoSet static_component_infos, const DynamicComponentSet& dynamic_components);
    void Detach(detail::ComponentInfoSet static_component_infos, const DynamicComponentSet& dynamic_components);
    auto GetArchetype() const noexcept -> Archetype&;

    EntityManager* m_EntityManager = nullptr;
    std::uint64_t  m_Id            = std::numeric_limits<std::uint64_t>::max();
};

template <Component T>
bool Entity::HasComponent() const noexcept {
    return HasComponent(utils::TypeID::Create<T>());
}

template <Component... Components>
    requires utils::unique_types<Components...> && utils::no_in<Entity, Components...>
void Entity::Attach(const DynamicComponentSet& dynamic_components) {
    Attach(detail::create_component_info_set<Components...>(), dynamic_components);
}

template <Component... Components>
    requires utils::unique_types<Components...> && utils::no_in<Entity, Components...>
void Entity::Detach(const DynamicComponentSet& dynamic_components) {
    Detach(detail::create_component_info_set<Components...>(), dynamic_components);
}

template <Component T>
auto Entity::GetComponent() const -> T& {
    auto ptr = GetComponent(utils::TypeID::Create<T>());
    return *reinterpret_cast<T*>(ptr);
}

}  // namespace hitagi::ecs

namespace std {
template <>
struct hash<hitagi::ecs::Entity> {
    constexpr std::size_t operator()(const hitagi::ecs::Entity& entity) const {
        return static_cast<std::size_t>(entity.m_Id);
    }
};
}  // namespace std