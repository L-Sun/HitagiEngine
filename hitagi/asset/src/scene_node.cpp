#include <hitagi/asset/scene_node.hpp>

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

}  // namespace hitagi::asset