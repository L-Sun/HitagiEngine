#include <hitagi/resource/camera.hpp>

#include <spdlog/spdlog.h>

using namespace hitagi::math;

namespace hitagi::resource {
Camera::Camera(float          aspect,
               float          near_clip,
               float          far_clip,
               float          fov,
               math::vec3f    position,
               math::vec3f    up,
               math::vec3f    look_at,
               allocator_type alloc)
    : SceneObject(alloc),
      m_Aspect(aspect),
      m_NearClipDistance(near_clip),
      m_FarClipDistance(far_clip),
      m_Fov(fov),
      m_Position(position),
      m_UpDirection(up),
      m_LookDirection(look_at),
      m_Transform(std::allocate_shared<Transform>(alloc)) {}

void Camera::SetAspect(float value) { m_Aspect = value; }
void Camera::SetNearClipDistance(float value) { m_NearClipDistance = value; }
void Camera::SetFarClipDistance(float value) { m_FarClipDistance = value; }
void Camera::SetFov(float value) { m_Fov = value; }
void Camera::SetTransform(std::shared_ptr<Transform> transform) {
    if (transform == nullptr) {
        if (auto logger = spdlog::get("AssetManager"); logger)
            logger->warn("you are setting a empty transform to camera! Nothing happend!");
        return;
    }
    m_Transform = std::move(transform);
}

auto Camera::GetTransform() const -> Transform& {
    assert(m_Transform != nullptr);
    return *m_Transform;
}

mat4f Camera::GetViewMatrix() const {
    return look_at(m_Position, m_LookDirection, m_UpDirection) * inverse(m_Transform->GetTransform());
}

float Camera::GetAspect() const noexcept { return m_Aspect; }

float Camera::GetNearClipDistance() const noexcept { return m_NearClipDistance; }

float Camera::GetFarClipDistance() const noexcept { return m_FarClipDistance; }

float Camera::GetFov() const noexcept { return m_Fov; }

}  // namespace hitagi::resource