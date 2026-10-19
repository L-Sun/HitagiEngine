export module ecs:entity;
import std;
import utils;

import :component;
import :entity_storage;

export namespace hitagi::ecs {

class EntityManager;

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
    bool     operator==(const Entity& rhs) const noexcept { return m_Storage == rhs.m_Storage && m_Id == rhs.m_Id; }
    bool     operator!=(const Entity& rhs) const noexcept { return !(*this == rhs); }

    friend struct std::formatter<hitagi::ecs::Entity>;
    friend std::hash<Entity>;

private:
    friend EntityManager;

    Entity(detail::EntityStorage* storage, entity_id_t id) : m_Storage(storage), m_Id(id) {}

    void CheckValidation() const;

    detail::EntityStorage* m_Storage = nullptr;
    entity_id_t            m_Id      = std::numeric_limits<entity_id_t>::max();
};

// Entity template implementations

template <Component T>
bool Entity::Has() const {
    CheckValidation();
    return m_Storage->HasComponent<T>(m_Id);
}

template <Component T, typename... Args>
    requires utils::not_same_as<T, Entity>
auto Entity::Emplace(Args&&... args) -> T& {
    CheckValidation();
    return m_Storage->EmplaceComponent<T>(m_Id, std::forward<Args>(args)...);
}

template <Component T>
    requires utils::not_same_as<T, Entity>
void Entity::Remove() {
    CheckValidation();
    m_Storage->RemoveComponent<T>(m_Id);
}

template <Component T>
    requires utils::not_same_as<T, Entity>
auto Entity::Get() -> T& {
    CheckValidation();
    return m_Storage->GetComponent<T>(m_Id);
}

template <Component T>
auto Entity::Get() const -> const T& {
    CheckValidation();
    return m_Storage->GetComponent<T>(m_Id);
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

namespace hitagi::ecs {

bool Entity::Has(std::string_view dynamic_component) const {
    CheckValidation();
    return m_Storage->HasDynamicComponent(m_Id, dynamic_component);
}

auto Entity::Get(std::string_view dynamic_component) -> std::byte* {
    CheckValidation();
    return m_Storage->GetDynamicComponent(m_Id, dynamic_component);
}

auto Entity::Get(std::string_view dynamic_component) const -> const std::byte* {
    CheckValidation();
    return m_Storage->GetDynamicComponent(m_Id, dynamic_component);
}

auto Entity::Add(std::string_view dynamic_component) -> std::byte* {
    CheckValidation();
    return m_Storage->AddDynamicComponent(m_Id, dynamic_component);
}

void Entity::Remove(std::string_view dynamic_component) {
    CheckValidation();
    m_Storage->RemoveDynamicComponent(m_Id, dynamic_component);
}

bool Entity::Valid() const noexcept {
    return m_Storage && m_Storage->Has(m_Id);
}

void Entity::CheckValidation() const {
    if (!Valid()) {
        throw std::runtime_error("Entity is not valid");
    }
}

}  // namespace hitagi::ecs
