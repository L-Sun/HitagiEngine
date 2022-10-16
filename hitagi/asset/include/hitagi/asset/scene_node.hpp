#pragma once
#include <hitagi/asset/transform.hpp>

#include <limits>
#include <set>
#include <memory>

namespace hitagi::asset {
class SceneNode : public std::enable_shared_from_this<SceneNode> {
public:
    SceneNode(std::string_view name = "", Transform transform = {})
        : name(name), transform(transform) {
    }

    inline void Attach(const std::shared_ptr<SceneNode>& parent) noexcept;
    inline void Detach() noexcept;

    virtual void Update();

    inline std::shared_ptr<SceneNode> GetParent() const noexcept { return m_Parent.lock(); }
    inline const auto&                GetChildren() const noexcept { return m_Children; }

    std::pmr::string name;
    Transform        transform;

protected:
    std::pmr::set<std::shared_ptr<SceneNode>> m_Children;
    std::weak_ptr<SceneNode>                  m_Parent;
};

template <typename T>
struct SceneNodeWithObject : public SceneNode {
    using SceneNode::SceneNode;
    std::shared_ptr<T> object;
};

inline void SceneNode::Attach(const std::shared_ptr<SceneNode>& parent) noexcept {
    if (parent.get() == this) {
        return;
    }
    Detach();
    if (parent) {
        m_Parent = parent;
        parent->m_Children.emplace(shared_from_this());
    }
}

inline void SceneNode::Detach() noexcept {
    if (auto parent = m_Parent.lock(); parent) {
        parent->m_Children.erase(shared_from_this());
    }
    m_Parent.reset();
}

inline void SceneNode::Update() {
    auto        parent        = m_Parent.lock();
    math::mat4f parent_matrix = parent ? parent->transform.world_matrix : math::mat4f::identity();
    transform.world_matrix    = parent_matrix * translate(transform.local_translation) * rotate(transform.local_rotation) * scale(transform.local_scaling);

    for (const auto& child : m_Children) {
        child->Update();
    }
};

}  // namespace hitagi::asset
