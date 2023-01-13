#include <hitagi/asset/scene_node.hpp>

using namespace hitagi::math;

namespace hitagi::asset {
SceneNode::SceneNode(Transform transform, std::string_view name, xg::Guid guid)
    : Resource(name, guid), transform(transform) {
}
void SceneNode::Attach(const std::shared_ptr<SceneNode>& parent) noexcept {
    if (parent.get() == this) {
        return;
    }
    Detach();
    if (parent) {
        m_Parent = parent;
        parent->m_Children.emplace(shared_from_this());
    }
}

void SceneNode::Detach() noexcept {
    if (auto parent = m_Parent.lock(); parent) {
        parent->m_Children.erase(shared_from_this());
    }
    m_Parent.reset();
}

void SceneNode::Update() {
    auto        parent        = m_Parent.lock();
    math::mat4f parent_matrix = parent ? parent->transform.world_matrix : math::mat4f::identity();
    transform.world_matrix    = parent_matrix * translate(transform.local_translation) * rotate(transform.local_rotation) * scale(transform.local_scaling);

    for (const auto& child : m_Children) {
        child->Update();
    }
};

void CameraNode::Update() {
    SceneNode::Update();
    const auto& camera_param = m_ObjectRef->parameters;

    auto [t, r, s]              = decompose(transform.world_matrix);
    math::vec3f global_eye      = (translate(t) * vec4f(camera_param.eye, 1.0f)).xyz;
    math::vec3f global_look_dir = (rotate(r) * vec4f(camera_param.look_dir, 0.0f)).xyz;
    math::vec3f global_up       = (rotate(r) * vec4f(camera_param.up, 0.0f)).xyz;

    m_View       = look_at(global_eye, global_look_dir, global_up);
    m_Projection = perspective(camera_param.horizontal_fov, camera_param.aspect, camera_param.near_clip, camera_param.far_clip);
    m_PV         = m_Projection * m_View;

    m_InvView       = inverse(m_View);
    m_InvProjection = inverse(m_Projection);
    m_InvPV         = inverse(m_PV);
}

math::vec3f LightNode::GetLightGlobalPosition() const {
    return (transform.world_matrix * vec4f(m_ObjectRef->parameters.position, 1.0f)).xyz;
}

math::vec3f LightNode::GetLightGlobalDirection() const {
    return (transform.world_matrix * vec4f(m_ObjectRef->parameters.direction, 0.0f)).xyz;
}

math::vec3f LightNode::GetLightGlobalUp() const {
    return (transform.world_matrix * vec4f(m_ObjectRef->parameters.up, 0.0f)).xyz;
}

}  // namespace hitagi::asset