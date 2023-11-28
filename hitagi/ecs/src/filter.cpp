#include <hitagi/ecs/filter.hpp>

#include "archetype.hpp"

#include <range/v3/algorithm/find.hpp>

namespace hitagi::ecs {

bool ComponentChecker::Exists(utils::TypeID component) const noexcept {
    return m_Archetype->HasComponent(component);
}

}  // namespace hitagi::ecs