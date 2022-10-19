#pragma once
#include <hitagi/asset/resource.hpp>
#include <hitagi/asset/transform.hpp>
#include <hitagi/asset/camera.hpp>
#include <hitagi/asset/light.hpp>

#include <set>

namespace hitagi::asset {
class SceneNode : public Resource, public std::enable_shared_from_this<SceneNode> {
public:
    SceneNode(Transform transform = {}, std::string_view name = "", xg::Guid guid = {});

    void Attach(const std::shared_ptr<SceneNode>& parent) noexcept;
    void Detach() noexcept;

    virtual void Update();

    inline std::shared_ptr<SceneNode> GetParent() const noexcept { return m_Parent.lock(); }
    inline const auto&                GetChildren() const noexcept { return m_Children; }

    Transform transform;

protected:
    std::pmr::set<std::shared_ptr<SceneNode>> m_Children;
    std::weak_ptr<SceneNode>                  m_Parent;
};

template <typename T>
class SceneNodeWithObject : public SceneNode {
public:
    SceneNodeWithObject(std::shared_ptr<T> obj_ref, Transform transform = {}, std::string_view name = "", xg::Guid guid = {})
        : SceneNode(transform, name, guid), m_ObjectRef(obj_ref) {}

    inline auto GetObjectRef() const noexcept { return m_ObjectRef; }

protected:
    std::shared_ptr<T> m_ObjectRef;
};

class CameraNode : public SceneNodeWithObject<Camera> {
public:
    using SceneNodeWithObject<Camera>::SceneNodeWithObject;

    inline math::mat4f GetView() const noexcept { return m_View; }
    inline math::mat4f GetProjection() const noexcept { return m_Projection; }
    inline math::mat4f GetProjectionView() const noexcept { return m_PV; }
    inline math::mat4f GetInvView() const noexcept { return m_InvView; }
    inline math::mat4f GetInvProjection() const noexcept { return m_InvProjection; }
    inline math::mat4f GetInvProjectionView() const noexcept { return m_InvPV; }

    void Update() final;

private:
    math::mat4f m_View, m_Projection, m_PV;
    math::mat4f m_InvView, m_InvProjection, m_InvPV;
};

class LightNode : public SceneNodeWithObject<Light> {
public:
    using SceneNodeWithObject<Light>::SceneNodeWithObject;

    math::vec3f GetLightGlobalPosition() const;
    math::vec3f GetLightGlobalDirection() const;
    math::vec3f GetLightGlobalUp() const;
};

}  // namespace hitagi::asset
