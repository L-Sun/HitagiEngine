#pragma once
#include <hitagi/resource/transform.hpp>

#include <limits>
#include <vector>
#include <memory>

namespace hitagi::resource {
class SceneNode : public std::enable_shared_from_this<SceneNode> {
public:
    SceneNode(std::string_view name = "", Transform transform = {})
        : name(name), transform(transform) {
    }

    inline void SetParent(const std::shared_ptr<SceneNode>& parent) noexcept;
    inline void AppendChild(const std::shared_ptr<SceneNode>& child) noexcept;

    virtual void Update();

    inline std::shared_ptr<SceneNode> GetParent() const noexcept { return m_Parent.lock(); }
    inline const auto&                GetChildren() const noexcept { return m_Children; }

    std::pmr::string name;
    Transform        transform;

private:
    std::pmr::vector<std::shared_ptr<SceneNode>> m_Children;
    std::weak_ptr<SceneNode>                     m_Parent;
};

template <typename T>
struct SceneNodeWithObject : public SceneNode {
    using SceneNode::SceneNode;
    std::shared_ptr<T> object;
};

inline void SceneNode::SetParent(const std::shared_ptr<SceneNode>& parent) noexcept {
    m_Parent = parent;
    if (parent) {
        parent->AppendChild(shared_from_this());
    }
}

inline void SceneNode::AppendChild(const std::shared_ptr<SceneNode>& child) noexcept {
    m_Children.emplace_back(child)->m_Parent = shared_from_this();
}

inline void SceneNode::Update() {
    auto        parent        = m_Parent.lock();
    math::mat4f parent_matrix = parent ? parent->transform.world_matrix : math::mat4f::identity();
    transform.world_matrix    = parent_matrix * translate(transform.local_translation) * rotate(transform.local_rotation) * scale(transform.local_scaling);

    for (const auto& child : m_Children) {
        child->Update();
    }
};

}  // namespace hitagi::resource
