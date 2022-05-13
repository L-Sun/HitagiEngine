#pragma once
#include <hitagi/resource/scene_object.hpp>
#include <hitagi/math/vector.hpp>

#include <numbers>

namespace hitagi::resource {
class Light : public SceneObject {
protected:
    Light(const math::vec4f& color = math::vec4f(1.0f), float intensity = 100.0f, allocator_type alloc = {})
        : SceneObject(alloc),
          m_LightColor(color),
          m_Intensity(intensity){};

    math::vec4f m_LightColor;
    float       m_Intensity;
    std::string m_TextureRef;

public:
    inline const auto& GetColor() const noexcept { return m_LightColor; }
    inline float       GetIntensity() const noexcept { return m_Intensity; }
};

class PointLight : public Light {
public:
    PointLight(const math::vec4f& color = math::vec4f(1.0f), float intensity = 100.0f, allocator_type alloc = {})
        : Light(color, intensity, alloc) {}
};

class SpotLight : public Light {
protected:
    math::vec3f m_Direction{};
    float       m_InnerConeAngle;
    float       m_OuterConeAngle;

public:
    SpotLight(const math::vec4f& color = math::vec4f(1.0f), float intensity = 100.0f,
              const math::vec3f& direction = math::vec3f(0.0f), float inner_cone_angle = std::numbers::pi / 3.0f,
              float outer_cone_angle = std::numbers::pi / 4.0f, allocator_type alloc = {})
        : Light(color, intensity, alloc), m_InnerConeAngle(inner_cone_angle), m_OuterConeAngle(outer_cone_angle) {}
};

class InfiniteLight : public Light {
public:
    using Light::Light;
};

}  // namespace hitagi::resource
