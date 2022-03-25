#include <hitagi/resource/camera.hpp>

namespace hitagi::asset {
std::ostream& operator<<(std::ostream& out, const Camera& obj) {
    out << static_cast<const SceneObject&>(obj) << std::endl;
    out << "Aspcet:             " << obj.m_Aspect << std::endl;
    out << "Fov:                " << obj.m_Fov << std::endl;
    out << "Near Clip Distance: " << obj.m_NearClipDistance << std::endl;
    out << "Far Clip Distance:  " << obj.m_FarClipDistance << std::endl;
    return out;
}
}  // namespace hitagi::asset