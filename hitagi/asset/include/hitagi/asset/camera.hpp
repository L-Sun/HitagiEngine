#pragma once
#include <hitagi/asset/scene_node.hpp>
#include <hitagi/asset/transform.hpp>

namespace hitagi::asset {
struct Camera {
public:
    float aspect    = 16.0f / 9.0f;
    float near_clip = 1.0f;
    float far_clip  = 1000.0f;
    float fov       = math::deg2rad(60.0f);

    // if camera is associated with Transform, then the following data will be updated by invoking `ApplyTransform`
    math::vec3f eye      = math::vec3f{2.0f, 2.0f, 2.0f};
    math::vec3f look_dir = math::vec3f{-1.0f, -1.0f, -1.0f};
    math::vec3f up       = math::vec3f{0.0f, 0.0f, 1.0f};

    void Update(const Transform& transform);

    math::vec4u        GetViewPort(std::uint32_t screen_width, std::uint32_t screen_height) const noexcept;
    inline math::mat4f GetView() const noexcept { return m_View; }
    inline math::mat4f GetProjection() const noexcept { return m_Projection; }
    inline math::mat4f GetProjectionView() const noexcept { return m_PV; }
    inline math::mat4f GetInvView() const noexcept { return m_InvView; }
    inline math::mat4f GetInvProjection() const noexcept { return m_InvProjection; }
    inline math::mat4f GetInvProjectionView() const noexcept { return m_InvPV; }

private:
    math::mat4f m_View, m_Projection, m_PV;
    math::mat4f m_InvView, m_InvProjection, m_InvPV;
};

template <>
struct SceneNodeWithObject<Camera> : public SceneNode {
    using SceneNode::SceneNode;
    std::shared_ptr<Camera> object;

    void Update() final;
};
using CameraNode = SceneNodeWithObject<Camera>;

}  // namespace hitagi::asset