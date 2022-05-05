#pragma once
#include <hitagi/resource/scene_object.hpp>

#include <numbers>

namespace hitagi::resource {
class Camera : public SceneObject {
public:
    Camera(float aspect = 16.0f / 9.0f, float near_clip = 1.0f, float far_clip = 1000.0f, float fov = 0.25 * std::numbers::pi)
        : m_Aspect(aspect),
          m_NearClipDistance(near_clip),
          m_FarClipDistance(far_clip),
          m_Fov(fov) {}

    inline void SetAspect(float value) { m_Aspect = value; }
    inline void SetNearClipDistance(float value) { m_NearClipDistance = value; }
    inline void SetFarClipDistance(float value) { m_FarClipDistance = value; }
    inline void SetFov(float value) { m_Fov = value; }

    inline float GetAspect() const noexcept { return m_Aspect; }
    inline float GetNearClipDistance() const noexcept { return m_NearClipDistance; }
    inline float GetFarClipDistance() const noexcept { return m_FarClipDistance; }
    inline float GetFov() const noexcept { return m_Fov; }

protected:
    float m_Aspect;
    float m_NearClipDistance;
    float m_FarClipDistance;
    float m_Fov;
};
}  // namespace hitagi::resource