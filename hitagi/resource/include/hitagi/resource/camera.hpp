#pragma once
#include <hitagi/resource/scene_object.hpp>
#include <hitagi/resource/transform.hpp>

#include <numbers>

namespace hitagi::resource {
class Camera : public SceneObject {
public:
    Camera(
        float       aspect    = 16.0f / 9.0f,
        float       near_clip = 1.0f,
        float       far_clip  = 1000.0f,
        float       fov       = 0.25 * std::numbers::pi,
        math::vec3f position  = math::vec3f{3.0f, 3.0f, 3.0f},
        math::vec3f up        = math::vec3f{0.0f, 0.0f, 1.0f},
        math::vec3f look_at   = math::vec3f{0.0f, 0.0f, 0.0f});

    void SetAspect(float value);
    void SetNearClipDistance(float value);
    void SetFarClipDistance(float value);
    void SetFov(float value);
    void SetTransform(std::shared_ptr<Transform> transform);

    Transform&  GetTransform() const;
    math::mat4f GetViewMatrix() const;
    math::vec3f GetGlobalPosition() const;
    float       GetAspect() const noexcept;
    float       GetNearClipDistance() const noexcept;
    float       GetFarClipDistance() const noexcept;
    float       GetFov() const noexcept;

protected:
    float m_Aspect;
    float m_NearClipDistance;
    float m_FarClipDistance;
    float m_Fov;

    math::vec3f m_Position;
    math::vec3f m_UpDirection;
    math::vec3f m_LookDirection;

    std::shared_ptr<Transform> m_Transform;
};

}  // namespace hitagi::resource