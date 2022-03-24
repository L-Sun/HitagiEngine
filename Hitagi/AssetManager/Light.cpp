#include "Light.hpp"

namespace hitagi::asset {
float default_atten_func(float intensity, float distance) { return intensity / pow(1 + distance, 2.0f); }

std::ostream& operator<<(std::ostream& out, const Light& obj) {
    out << static_cast<const SceneObject&>(obj) << std::endl;
    out << "Color:        " << obj.m_LightColor << std::endl;
    out << "Intensity:    " << obj.m_Intensity << std::endl;
    out << "Texture:      " << obj.m_TextureRef;
    return out;
}
std::ostream& operator<<(std::ostream& out, const PointLight& obj) {
    out << static_cast<const Light&>(obj) << std::endl;
    out << "Light Type: Omni" << std::endl;
    return out;
}
std::ostream& operator<<(std::ostream& out, const SpotLight& obj) {
    out << static_cast<const Light&>(obj) << std::endl;
    out << "Light Type:       Spot" << std::endl;
    out << "Inner Cone Angle: " << obj.m_InnerConeAngle << std::endl;
    out << "Outer Cone Angle: " << obj.m_OuterConeAngle;
    return out;
}
std::ostream& operator<<(std::ostream& out, const InfiniteLight& obj) {
    out << static_cast<const Light&>(obj) << std::endl;
    out << "Light Type: Infinite" << std::endl;
    return out;
}
}  // namespace hitagi::asset