#pragma once
#include <hitagi/ecs/entity_manager.hpp>

#include <functional>

namespace hitagi::ecs {

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

    friend auto format_as(const Entity& entity) noexcept { return entity.m_Id; }
    friend std::hash<Entity>;

private:
    friend EntityManager;

    Entity(EntityManager* manager, entity_id_t id) : m_EntityManager(manager), m_Id(id) {}

    void CheckValidation() const;

    EntityManager* m_EntityManager = nullptr;
    entity_id_t    m_Id            = std::numeric_limits<entity_id_t>::max();
};

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

namespace std {
template <>
struct hash<hitagi::ecs::Entity> {
    constexpr std::size_t operator()(const hitagi::ecs::Entity& entity) const {
        return static_cast<std::size_t>(entity.m_Id);
    }
};
}  // namespace std
