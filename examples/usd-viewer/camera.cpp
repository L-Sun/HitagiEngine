export module viewer:camera;

import std;
import asset;
import math;
import hid;

export namespace hitagi::examples {

class OrbitController {
public:
    auto Eye() const -> math::vec3f { return m_Eye; }
    auto LookDirection() const -> math::vec3f { return m_LookDirection; }
    void Fit(asset::CameraComponent& camera, const math::AABBf& bounds) {
        m_Center = bounds.Center();
        m_Radius = std::max(bounds.Extents().norm(), 0.01f);
        m_Zoom   = 1.0f;
        m_Yaw    = 0.35f;
        m_Pitch  = 0.2f;

        camera.near_clip = m_Radius * 0.001f;
        camera.far_clip  = m_Radius * 200.0f;
        UpdatePose(camera);
    }

    void Update(asset::CameraComponent& camera, const hid::InputState& input, float aspect, float viewport_height,
                bool enabled = true) {
        using Key = hid::VirtualKeyCode;
        if (input.GetBoolNew(Key::MOUSE_L_BUTTON)) m_OrbitDrag = enabled;
        if (input.GetBoolNew(Key::MOUSE_M_BUTTON)) m_PanDrag = enabled;
        if (!input.GetBool(Key::MOUSE_L_BUTTON)) m_OrbitDrag = false;
        if (!input.GetBool(Key::MOUSE_M_BUTTON)) m_PanDrag = false;
        if (enabled && m_OrbitDrag && !input.GetBoolNew(Key::MOUSE_L_BUTTON)) {
            m_Yaw -= input.GetFloatDelta(hid::MouseEvent::MOVE_X) * 0.008f;
            m_Pitch += input.GetFloatDelta(hid::MouseEvent::MOVE_Y) * 0.008f;
            m_Pitch = std::clamp(m_Pitch, -1.5f, 1.5f);
        }
        if (enabled && m_PanDrag && !input.GetBoolNew(Key::MOUSE_M_BUTTON)) {
            const auto& parameters = camera;
            const auto  right      = math::normalize(math::cross(m_LookDirection, math::vec3f(0, 1, 0)));
            const auto  up         = math::normalize(math::cross(right, m_LookDirection));
            const float scale      = 2.0f * m_LookDirection.norm() *
                                     std::tan(parameters.horizontal_fov * 0.5f) /
                                     (aspect * std::max(viewport_height, 1.0f));
            m_Center += (up * input.GetFloatDelta(hid::MouseEvent::MOVE_Y) -
                         right * input.GetFloatDelta(hid::MouseEvent::MOVE_X)) *
                        scale;
        }
        const float wheel = enabled ? input.GetFloatDelta(hid::MouseEvent::SCROLL_Y) : 0.0f;
        m_Zoom            = std::clamp(m_Zoom * std::exp(-wheel * 0.15f), 0.1f, 20.0f);

        camera.aspect = aspect;
        UpdatePose(camera);
    }

    void SetView(asset::CameraComponent& camera, std::string_view view) {
        m_Yaw   = view == "right" ? 1.5707963f : view == "home" ? 0.35f
                                                                : 0.0f;
        m_Pitch = view == "top" ? 1.5f : view == "home" ? 0.2f
                                                        : 0.0f;
        UpdatePose(camera);
    }

private:
    void UpdatePose(asset::CameraComponent& camera) {
        auto& parameters = camera;
        // Fit a bounding sphere to the smaller horizontal/vertical field of view.
        const float half_fov =
            std::atan(std::tan(parameters.horizontal_fov * 0.5f) / std::max(parameters.aspect, 1.0f));
        const float       distance = m_Radius * 1.15f * m_Zoom / std::sin(half_fov);
        const math::vec3f direction{
            std::sin(m_Yaw) * std::cos(m_Pitch),
            std::sin(m_Pitch),
            std::cos(m_Yaw) * std::cos(m_Pitch),
        };
        m_Eye           = m_Center + direction * distance;
        m_LookDirection = m_Center - m_Eye;
    }

    math::vec3f m_Center{};
    math::vec3f m_Eye{0, -1, 0};
    math::vec3f m_LookDirection{0, 1, 0};
    float       m_Radius    = 1.0f;
    float       m_Zoom      = 1.0f;
    float       m_Yaw       = 0.35f;
    float       m_Pitch     = 0.2f;
    bool        m_OrbitDrag = false;
    bool        m_PanDrag   = false;
};

}  // namespace hitagi::examples
