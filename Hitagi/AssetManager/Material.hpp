#pragma once
#include "SceneObject.hpp"
#include "Texture.hpp"
#include "HitagiMath.hpp"

#include <crossguid/guid.hpp>

#include <variant>

namespace Hitagi::Asset {
class Material : public SceneObject {
public:
    template <typename T>
    using Parameter = std::variant<T, std::shared_ptr<Texture>>;

    using Color       = Parameter<vec4f>;
    using Normal      = Parameter<vec3f>;
    using SingleValue = Parameter<float>;

    Material() : m_AmbientColor(vec4f(0.0f)),
                 m_DiffuseColor(vec4f(1.0f)),
                 m_Metallic(0.0f),
                 m_Roughness(0.0f),
                 m_Normal(vec3f(0.0f, 0.0f, 1.0f)),
                 m_Specular(vec4f(0.0f)),
                 m_SpecularPower(1.0f),
                 m_AmbientOcclusion(1.0f),
                 m_Opacity(1.0f),
                 m_Transparency(vec4f(0.0f)),
                 m_Emission(vec4f(0.0f)) {}

    inline const Color&       GetAmbientColor() const noexcept { return m_AmbientColor; }
    inline const Color&       GetDiffuseColor() const noexcept { return m_DiffuseColor; }
    inline const Color&       GetSpecularColor() const noexcept { return m_Specular; }
    inline const SingleValue& GetSpecularPower() const noexcept { return m_SpecularPower; }
    inline const Color&       GetEmission() const noexcept { return m_Emission; }

    void SetColor(std::string_view attrib, const vec4f& color);
    void SetParam(std::string_view attrib, const float param);
    void SetTexture(std::string_view attrib, const std::shared_ptr<Texture>& texture);
    void LoadTextures();

    friend std::ostream& operator<<(std::ostream& out, const Material& obj);

protected:
    Color       m_AmbientColor;
    Color       m_DiffuseColor;
    SingleValue m_Metallic;
    SingleValue m_Roughness;
    Normal      m_Normal;
    Color       m_Specular;
    SingleValue m_SpecularPower;
    SingleValue m_AmbientOcclusion;
    SingleValue m_Opacity;
    Color       m_Transparency;
    Color       m_Emission;
};

template <typename T>
inline std::ostream& operator<<(std::ostream& out, const Material::Parameter<T>& param) {
    if (std::holds_alternative<T>(param))
        return out << std::get<T>(param);
    else
        return out << *std::get<std::shared_ptr<Texture>>(param);
}

}  // namespace Hitagi::Asset