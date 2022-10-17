#include <hitagi/asset/camera.hpp>

using namespace hitagi::math;

namespace hitagi::asset {

void Camera::Update(const Transform& transform) {
    auto [t, r, s]        = decompose(transform.world_matrix);
    vec3f global_eye      = (translate(t) * vec4f(eye, 1)).xyz;
    vec3f global_look_dir = (rotate(r) * vec4f(look_dir, 0)).xyz;
    vec3f global_up       = (rotate(r) * vec4f(up, 0)).xyz;

    m_View       = look_at(global_eye, global_look_dir, global_up);
    m_Projection = perspective(fov, aspect, near_clip, far_clip);
    m_PV         = m_Projection * m_View;

    m_InvView       = inverse(m_View);
    m_InvProjection = inverse(m_Projection);
    m_InvPV         = inverse(m_PV);
}

math::vec4u Camera::GetViewPort(std::uint32_t screen_width, std::uint32_t screen_height) const noexcept {
    math::vec4u view_port;
    {
        std::uint32_t height = screen_height;
        std::uint32_t width  = height * aspect;
        if (width > screen_width) {
            width     = screen_width;
            height    = screen_width / aspect;
            view_port = {0, (screen_height - height) >> 1, width, height};
        } else {
            view_port = {(screen_width - width) >> 1, 0, width, height};
        }
    }
    return view_port;
}

void CameraNode::Update() {
    SceneNode::Update();
    if (object) {
        object->Update(transform);
    }
}

}  // namespace hitagi::asset