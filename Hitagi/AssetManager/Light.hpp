#pragma once
#include "SceneObject.hpp"

#include <Hitagi/Math/Vector.hpp>

#include <numbers>

namespace hitagi::asset {
using AttenFunc = std::function<float(float, float)>;
float default_atten_func(float intensity, float distance);

class Light : public SceneObject {
protected:
    Light(const math::vec4f& color = math::vec4f(1.0f), float intensity = 100.0f)
        : m_LightColor(color),
          m_Intensity(intensity),
          m_LightAttenuation(default_atten_func){};

    math::vec4f m_LightColor;
    float       m_Intensity;
    AttenFunc   m_LightAttenuation;
    std::string m_TextureRef;

    friend std::ostream& operator<<(std::ostream& out, const Light& obj);

public:
    inline void        SetAttenuation(AttenFunc func) noexcept { m_LightAttenuation = func; }
    inline const auto& GetColor() const noexcept { return m_LightColor; }
    inline float       GetIntensity() const noexcept { return m_Intensity; }
};

class PointLight : public Light {
public:
    PointLight(const math::vec4f& color = math::vec4f(1.0f), float intensity = 100.0f)
        : Light(color, intensity) {}

    friend std::ostream& operator<<(std::ostream& out, const PointLight& obj);
};

class SpotLight : public Light {
protected:
    math::vec3f m_Direction{};
    float       m_InnerConeAngle;
    float       m_OuterConeAngle;

public:
    SpotLight(const math::vec4f& color = math::vec4f(1.0f), float intensity = 100.0f,
              const math::vec3f& direction = math::vec3f(0.0f), float inner_cone_angle = std::numbers::pi / 3.0f,
              float outer_cone_angle = std::numbers::pi / 4.0f)
        : Light(color, intensity), m_InnerConeAngle(inner_cone_angle), m_OuterConeAngle(outer_cone_angle) {}

    friend std::ostream& operator<<(std::ostream& out, const SpotLight& obj);
};

class InfiniteLight : public Light {
public:
    using Light::Light;
    friend std::ostream& operator<<(std::ostream& out, const InfiniteLight& obj);
};

}  // namespace hitagi::asset
