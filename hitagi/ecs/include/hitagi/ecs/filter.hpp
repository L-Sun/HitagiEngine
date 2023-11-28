#pragma once
#include <hitagi/ecs/component.hpp>
#include <hitagi/utils/types.hpp>

namespace hitagi::ecs {
class Archetype;

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

}  // namespace hitagi::ecs