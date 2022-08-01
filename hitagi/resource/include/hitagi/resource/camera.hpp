#pragma once
#include <hitagi/resource/transform.hpp>

#include <numbers>

namespace hitagi::resource {
struct Camera {
public:
    float aspect    = 16.0f / 9.0f;
    float near_clip = 1.0f;
    float far_clip  = 1000.0f;
    float fov       = 0.25 * std::numbers::pi;

    // if camera is associated with Transform, then the following data will be updated by invoking `ApplyTransform`
    math::vec3f eye      = math::vec3f(0.0f);
    math::vec3f look_dir = math::vec3f{0.0f, 1.0f, 0.0f};
    math::vec3f up       = math::vec3f{0.0f, 0.0f, 1.0f};

    void Update();
    void ApplyTransform(const Transform& transform);

    inline bool        IsDirty() const noexcept { return m_Dirty; }
    inline math::mat4f GetView() const noexcept { return m_View; }
    inline math::mat4f GetProjection() const noexcept { return m_Projection; }
    inline math::mat4f GetProjectionView() const noexcept { return m_PV; }
    inline math::mat4f GetInvView() const noexcept { return m_InvView; }
    inline math::mat4f GetInvProjection() const noexcept { return m_InvProjection; }
    inline math::mat4f GetInvProjectionView() const noexcept { return m_InvPV; }

private:
    bool        m_Dirty = false;
    math::mat4f m_View, m_Projection, m_PV;
    math::mat4f m_InvView, m_InvProjection, m_InvPV;
};

}  // namespace hitagi::resource