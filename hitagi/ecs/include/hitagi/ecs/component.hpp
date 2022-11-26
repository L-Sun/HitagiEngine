#pragma once
#include <functional>
#include <hitagi/utils/concepts.hpp>

#include <string>
#include <set>

namespace hitagi::ecs {

template <typename T>
concept Component = utils::NoCVRef<T> && std::is_default_constructible_v<T>;
struct DynamicComponent {
    std::pmr::string name;
    std::size_t      size;

    std::function<void(void*)> constructor, deconstructor;

    constexpr auto operator<=>(const DynamicComponent& rhs) const {
        return name <=> rhs.name;
    };
};
using DynamicComponents = std::pmr::set<DynamicComponent>;

}  // namespace hitagi::ecs