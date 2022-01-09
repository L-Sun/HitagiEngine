#include "SceneObject.hpp"

namespace Hitagi::Asset {
std::ostream& operator<<(std::ostream& out, const SceneObject& obj) {
    out << "GUID: " << obj.m_Guid << std::dec << std::endl;
    return out;
}
}  // namespace Hitagi::Asset