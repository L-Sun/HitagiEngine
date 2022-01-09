#pragma once
#include "SceneObject.hpp"

namespace Hitagi::Asset {
using AttenFunc = std::function<float(float, float)>;
float default_atten_func(float intensity, float distance);

class Light : public SceneObject {
protected:
    Light(const vec4f& color = vec4f(1.0f), float intensity = 100.0f)
        : m_LightColor(color),
          m_Intensity(intensity),
          m_LightAttenuation(default_atten_func){};

    vec4f       m_LightColor;
    float       m_Intensity;
    AttenFunc   m_LightAttenuation;
    std::string m_TextureRef;

    friend std::ostream& operator<<(std::ostream& out, const Light& obj);

public:
    inline void         SetAttenuation(AttenFunc func) noexcept { m_LightAttenuation = func; }
    inline const vec4f& GetColor() const noexcept { return m_LightColor; }
    inline float        GetIntensity() const noexcept { return m_Intensity; }
};

class PointLight : public Light {
public:
    PointLight(const vec4f& color = vec4f(1.0f), float intensity = 100.0f)
        : Light(color, intensity) {}

    friend std::ostream& operator<<(std::ostream& out, const PointLight& obj);
};

class SpotLight : public Light {
protected:
    vec3f m_Direction{};
    float m_InnerConeAngle;
    float m_OuterConeAngle;

public:
    SpotLight(const vec4f& color = vec4f(1.0f), float intensity = 100.0f,
              const vec3f& direction = vec3f(0.0f), float inner_cone_angle = std::numbers::pi / 3.0f,
              float outer_cone_angle = std::numbers::pi / 4.0f)
        : Light(color, intensity), m_InnerConeAngle(inner_cone_angle), m_OuterConeAngle(outer_cone_angle) {}

    friend std::ostream& operator<<(std::ostream& out, const SpotLight& obj);
};

class InfiniteLight : public Light {
public:
    using Light::Light;
    friend std::ostream& operator<<(std::ostream& out, const InfiniteLight& obj);
};

}  // namespace Hitagi::Asset
