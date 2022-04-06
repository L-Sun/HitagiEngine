#include <hitagi/resource/material.hpp>

using namespace hitagi::math;

namespace hitagi::asset {
void Material::SetColor(std::string_view attrib, const vec4f& color) {
    if (attrib == "ambient") m_AmbientColor = color;
    if (attrib == "diffuse") m_DiffuseColor = color;
    if (attrib == "specular") m_Specular = color;
    if (attrib == "emission") m_Emission = color;
    if (attrib == "transparency") m_Transparency = color;
}
void Material::SetParam(std::string_view attrib, const float param) {
    if (attrib == "specular_power") m_SpecularPower = param;
    if (attrib == "opacity") m_Opacity = param;
}
void Material::SetTexture(std::string_view attrib, const std::shared_ptr<Texture>& texture) {
    if (attrib == "ambient") m_DiffuseColor = texture;
    if (attrib == "diffuse") m_DiffuseColor = texture;
    if (attrib == "specular") m_Specular = texture;
    if (attrib == "specular_power") m_SpecularPower = texture;
    if (attrib == "emission") m_Emission = texture;
    if (attrib == "opacity") m_Opacity = texture;
    if (attrib == "transparency") m_Transparency = texture;
    if (attrib == "normal") m_Normal = texture;
}
void Material::LoadTextures() {
    if (std::holds_alternative<std::shared_ptr<Texture>>(m_DiffuseColor))
        std::get<std::shared_ptr<Texture>>(m_DiffuseColor)->LoadTexture();
}
std::ostream& operator<<(std::ostream& out, const Material& obj) {
    out << "Albedo:            " << obj.m_DiffuseColor << std::endl;
    out << "Metallic:          " << obj.m_Metallic << std::endl;
    out << "Roughness:         " << obj.m_Roughness << std::endl;
    out << "Normal:            " << obj.m_Normal << std::endl;
    out << "Specular:          " << obj.m_Specular << std::endl;
    out << "Ambient Occlusion: " << obj.m_AmbientOcclusion;
    return out;
}
}  // namespace hitagi::asset