module ecs;
import std;

namespace hitagi::ecs {

bool ComponentChecker::Exists(utils::TypeID component) const noexcept {
    return m_Archetype->HasComponent(component);
}

}  // namespace hitagi::ecs
