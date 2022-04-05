#pragma once
#include "scene_object.hpp"
#include "texture.hpp"

#include <hitagi/math/vector.hpp>

#include <crossguid/guid.hpp>

#include <variant>

namespace hitagi::resource {
class Material : public SceneObject {
public:
    template <typename T>
    using Parameter = std::variant<T, std::shared_ptr<Texture>>;

    using Color       = Parameter<math::vec4f>;
    using Normal      = Parameter<math::vec3f>;
    using SingleValue = Parameter<float>;

    Material() : m_AmbientColor(math::vec4f(0.0f)),
                 m_DiffuseColor(math::vec4f(1.0f)),
                 m_Metallic(0.0f),
                 m_Roughness(0.0f),
                 m_Normal(math::vec3f(0.0f, 0.0f, 1.0f)),
                 m_Specular(math::vec4f(0.0f)),
                 m_SpecularPower(1.0f),
                 m_AmbientOcclusion(1.0f),
                 m_Opacity(1.0f),
                 m_Transparency(math::vec4f(0.0f)),
                 m_Emission(math::vec4f(0.0f)) {}

    inline const Color&       GetAmbientColor() const noexcept { return m_AmbientColor; }
    inline const Color&       GetDiffuseColor() const noexcept { return m_DiffuseColor; }
    inline const Color&       GetSpecularColor() const noexcept { return m_Specular; }
    inline const SingleValue& GetSpecularPower() const noexcept { return m_SpecularPower; }
    inline const Color&       GetEmission() const noexcept { return m_Emission; }

    void SetColor(std::string_view attrib, const math::vec4f& color);
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

}  // namespace hitagi::resource