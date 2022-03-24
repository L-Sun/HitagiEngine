#include "SceneObject.hpp"

namespace hitagi::asset {
std::ostream& operator<<(std::ostream& out, const SceneObject& obj) {
    out << "GUID: " << obj.m_Guid << std::dec << std::endl;
    return out;
}
}  // namespace hitagi::asset