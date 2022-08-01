#include <hitagi/resource/camera.hpp>

using namespace hitagi::math;

namespace hitagi::resource {
void Camera::Update() {
    m_View       = look_at(eye, look_dir, up);
    m_Projection = perspective(fov, aspect, near_clip, far_clip);
    m_PV         = m_Projection * m_View;

    m_InvView       = inverse(m_View);
    m_InvProjection = inverse(m_Projection);
    m_InvPV         = inverse(m_PV);
}

void Camera::ApplyTransform(const Transform& transform) {
    eye      = (transform.world_matrix * vec4f(1, 1, 1, 1)).xyz;
    look_dir = normalize(transform.world_matrix * vec4f(-1, -1, -1, 0)).xyz;
    up       = normalize(transform.world_matrix * vec4f(0, 0, 1, 0)).xyz;

    m_View = look_at(eye, look_dir, up);
}

}  // namespace hitagi::resource