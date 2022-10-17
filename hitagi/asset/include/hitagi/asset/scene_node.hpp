#pragma once
#include <hitagi/asset/resource.hpp>
#include <hitagi/asset/transform.hpp>

#include <limits>
#include <set>
#include <memory>

namespace hitagi::asset {
class SceneNode : public Resource, public std::enable_shared_from_this<SceneNode> {
public:
    SceneNode(Transform transform = {}, std::string_view name = "", xg::Guid guid = {});

    void Attach(const std::shared_ptr<SceneNode>& parent) noexcept;
    void Detach() noexcept;

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

}  // namespace hitagi::asset
