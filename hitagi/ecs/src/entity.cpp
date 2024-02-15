#include <hitagi/ecs/entity.hpp>
#include <hitagi/ecs/entity_manager.hpp>

#include <range/v3/view/transform.hpp>
#include <range/v3/range/conversion.hpp>

namespace hitagi::ecs {

bool Entity::Has(std::string_view dynamic_component) const {
    CheckValidation();
    return m_EntityManager->HasDynamicComponent(m_Id, dynamic_component);
}

auto Entity::Get(std::string_view dynamic_component) -> std::byte* {
    CheckValidation();
    return m_EntityManager->GetDynamicComponent(m_Id, dynamic_component);
}

auto Entity::Get(std::string_view dynamic_component) const -> const std::byte* {
    CheckValidation();
    return m_EntityManager->GetDynamicComponent(m_Id, dynamic_component);
}

auto Entity::Add(std::string_view dynamic_component) -> std::byte* {
    CheckValidation();
    return m_EntityManager->AddDynamicComponent(m_Id, dynamic_component);
}

void Entity::Remove(std::string_view dynamic_component) {
    CheckValidation();
    m_EntityManager->RemoveDynamicComponent(m_Id, dynamic_component);
}

bool Entity::Valid() const noexcept {
    return m_EntityManager && m_EntityManager->Has(*this);
}

void Entity::CheckValidation() const {
    if (!Valid()) {
        throw std::runtime_error("Entity is not valid");
    }
}

}  // namespace hitagi::ecs